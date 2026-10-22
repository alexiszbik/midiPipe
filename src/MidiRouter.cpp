#include "MidiRouter.h"

#include <algorithm>

namespace
{
void formatRtMidiError(std::string& out, const char* context, const RtMidiError& error)
{
    out = context;
    out += ": ";
    out += error.what();
}
} // namespace

MidiRouter::MidiRouter() = default;

MidiRouter::~MidiRouter()
{
    stop();
}

void MidiRouter::start()
{
    if (running_)
        return;

    running_ = true;
    refreshPorts();
    appendLog("Router started.");
}

void MidiRouter::stop()
{
    if (!running_ && inputs_.empty() && outputs_.empty())
        return;

    std::lock_guard<std::mutex> lock(stateMutex_);

    for (auto& input : inputs_)
        closeInput(input);
    for (auto& output : outputs_)
        closeOutput(output);

    inputs_.clear();
    outputs_.clear();
    running_ = false;
}

void MidiRouter::refreshPorts()
{
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    try
    {
        inputNames = enumerateInputNames();
        outputNames = enumerateOutputNames();
    }
    catch (const RtMidiError& error)
    {
        std::string message;
        formatRtMidiError(message, "Port enumeration failed", error);
        appendLog(message);
        return;
    }

    std::lock_guard<std::mutex> lock(stateMutex_);

    for (auto it = inputs_.begin(); it != inputs_.end();)
    {
        const bool stillListed = std::find(inputNames.begin(), inputNames.end(), it->portName)
                                 != inputNames.end();
        const bool open = it->midiIn && it->midiIn->isPortOpen();

        if (!stillListed || !open)
        {
            if (it->portName.empty() == false)
            {
                appendLog(open ? ("Input disconnected: " + it->portName)
                               : ("Input closed: " + it->portName));
            }
            closeInput(*it);
            it = inputs_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = outputs_.begin(); it != outputs_.end();)
    {
        const bool stillListed = std::find(outputNames.begin(), outputNames.end(), it->portName)
                                 != outputNames.end();
        const bool open = it->midiOut && it->midiOut->isPortOpen();

        if (!stillListed || !open)
        {
            if (!it->portName.empty())
            {
                appendLog(open ? ("Output disconnected: " + it->portName)
                               : ("Output closed: " + it->portName));
            }
            closeOutput(*it);
            it = outputs_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& name : inputNames)
    {
        if (!isInputConnected(name))
            openInput(name);
    }

    for (const auto& name : outputNames)
    {
        if (!isOutputConnected(name))
            openOutput(name);
    }
}

std::vector<MidiPortView> MidiRouter::inputPorts() const
{
    std::vector<std::string> names;
    try
    {
        names = enumerateInputNames();
    }
    catch (const RtMidiError&)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<MidiPortView> ports;
    ports.reserve(names.size());

    for (const auto& name : names)
    {
        MidiPortView view;
        view.name = name;
        view.connected = isInputConnected(name);
        ports.push_back(std::move(view));
    }

    return ports;
}

std::vector<MidiPortView> MidiRouter::outputPorts() const
{
    std::vector<std::string> names;
    try
    {
        names = enumerateOutputNames();
    }
    catch (const RtMidiError&)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<MidiPortView> ports;
    ports.reserve(names.size());

    for (const auto& name : names)
    {
        MidiPortView view;
        view.name = name;
        view.connected = isOutputConnected(name);
        ports.push_back(std::move(view));
    }

    return ports;
}

std::vector<std::string> MidiRouter::recentLogLines() const
{
    std::lock_guard<std::mutex> lock(logMutex_);
    return { logLines_.begin(), logLines_.end() };
}

std::vector<std::string> MidiRouter::enumerateInputNames()
{
    RtMidiIn probe;
    const auto count = probe.getPortCount();
    std::vector<std::string> names;
    names.reserve(count);

    for (unsigned int i = 0; i < count; ++i)
        names.push_back(probe.getPortName(i));

    return names;
}

std::vector<std::string> MidiRouter::enumerateOutputNames()
{
    RtMidiOut probe;
    const auto count = probe.getPortCount();
    std::vector<std::string> names;
    names.reserve(count);

    for (unsigned int i = 0; i < count; ++i)
        names.push_back(probe.getPortName(i));

    return names;
}

int MidiRouter::findPortIndexByName(RtMidiIn& probe, const std::string& name)
{
    const auto count = probe.getPortCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (probe.getPortName(i) == name)
            return static_cast<int>(i);
    }
    return -1;
}

int MidiRouter::findPortIndexByName(RtMidiOut& probe, const std::string& name)
{
    const auto count = probe.getPortCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (probe.getPortName(i) == name)
            return static_cast<int>(i);
    }
    return -1;
}

void MidiRouter::closeInput(InputPort& input)
{
    if (!input.midiIn)
        return;

    try
    {
        if (input.midiIn->isPortOpen())
            input.midiIn->cancelCallback();
    }
    catch (const RtMidiError&)
    {
    }

    try
    {
        if (input.midiIn->isPortOpen())
            input.midiIn->closePort();
    }
    catch (const RtMidiError&)
    {
    }
}

void MidiRouter::closeOutput(OutputPort& output)
{
    if (!output.midiOut)
        return;

    try
    {
        if (output.midiOut->isPortOpen())
            output.midiOut->closePort();
    }
    catch (const RtMidiError&)
    {
    }
}

void MidiRouter::openInput(const std::string& portName)
{
    try
    {
        RtMidiIn portList;
        const int portIndex = findPortIndexByName(portList, portName);
        if (portIndex < 0)
            return;

        auto midiIn = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, kClientName, 0);
        midiIn->ignoreTypes(false, false, false);
        midiIn->setCallback(&MidiRouter::midiCallback, this);
        midiIn->openPort(static_cast<unsigned int>(portIndex), kClientName);

        InputPort input;
        input.midiIn = std::move(midiIn);
        input.portName = portName;
        inputs_.push_back(std::move(input));

        appendLog("Opened input: " + portName);
    }
    catch (const RtMidiError& error)
    {
        std::string message;
        formatRtMidiError(message, "Failed to open input", error);
        appendLog(message);
    }
}

void MidiRouter::openOutput(const std::string& portName)
{
    try
    {
        RtMidiOut portList;
        const int portIndex = findPortIndexByName(portList, portName);
        if (portIndex < 0)
            return;

        auto midiOut = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, kClientName);
        midiOut->openPort(static_cast<unsigned int>(portIndex), kClientName);

        OutputPort output;
        output.midiOut = std::move(midiOut);
        output.portName = portName;
        outputs_.push_back(std::move(output));

        appendLog("Opened output: " + portName);
    }
    catch (const RtMidiError& error)
    {
        std::string message;
        formatRtMidiError(message, "Failed to open output", error);
        appendLog(message);
    }
}

void MidiRouter::appendLog(const std::string& line)
{
    std::lock_guard<std::mutex> lock(logMutex_);
    logLines_.push_back(line);
    while (logLines_.size() > kMaxLogLines)
        logLines_.pop_front();
}

bool MidiRouter::isInputConnected(const std::string& name) const
{
    for (const auto& input : inputs_)
    {
        if (input.portName == name && input.midiIn && input.midiIn->isPortOpen())
            return true;
    }
    return false;
}

bool MidiRouter::isOutputConnected(const std::string& name) const
{
    for (const auto& output : outputs_)
    {
        if (output.portName == name && output.midiOut && output.midiOut->isPortOpen())
            return true;
    }
    return false;
}

void MidiRouter::midiCallback(double /*timeStamp*/,
                              std::vector<unsigned char>* message,
                              void* userData)
{
    if (message == nullptr || userData == nullptr)
        return;

    auto* router = static_cast<MidiRouter*>(userData);
    router->handleIncomingMessage(*message);
}

void MidiRouter::handleIncomingMessage(const std::vector<unsigned char>& message)
{
    if (message.empty()) {
        return;
    }

    std::vector<RtMidiOut*> outputs;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        outputs.reserve(outputs_.size());
        for (const auto& output : outputs_)
        {
            if (output.midiOut && output.midiOut->isPortOpen()) {
                outputs.push_back(output.midiOut.get());
            }
        }
    }

    for (auto* midiOut : outputs)
    {
        try
        {
            midiOut->sendMessage(&message);
        }
        catch (const RtMidiError& error)
        {
            std::string logMessage;
            formatRtMidiError(logMessage, "Failed to send MIDI", error);
            appendLog(logMessage);
        }
    }
}
