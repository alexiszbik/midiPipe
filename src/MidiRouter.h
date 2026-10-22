#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "RtMidi.h"

struct MidiPortView
{
    std::string name;
    bool connected = false;
};

class MidiRouter
{
public:
    static constexpr const char* kClientName = "midiPipe";

    MidiRouter();
    ~MidiRouter();

    MidiRouter(const MidiRouter&) = delete;
    MidiRouter& operator=(const MidiRouter&) = delete;

    void start();
    void stop();

    void refreshPorts();

    [[nodiscard]] bool isRunning() const noexcept { return running_; }

    [[nodiscard]] std::vector<MidiPortView> inputPorts() const;
    [[nodiscard]] std::vector<MidiPortView> outputPorts() const;
    [[nodiscard]] std::vector<std::string> recentLogLines() const;

private:
    struct InputPort
    {
        std::unique_ptr<RtMidiIn> midiIn;
        std::string portName;
    };

    struct OutputPort
    {
        std::unique_ptr<RtMidiOut> midiOut;
        std::string portName;
    };

    static void midiCallback(double timeStamp,
                             std::vector<unsigned char>* message,
                             void* userData);

    void handleIncomingMessage(const std::vector<unsigned char>& message);

    [[nodiscard]] static std::vector<std::string> enumerateInputNames();
    [[nodiscard]] static std::vector<std::string> enumerateOutputNames();
    [[nodiscard]] static int findPortIndexByName(RtMidiIn& probe,
                                                 const std::string& name);
    [[nodiscard]] static int findPortIndexByName(RtMidiOut& probe,
                                                 const std::string& name);

    void closeInput(InputPort& input);
    void closeOutput(OutputPort& output);
    void openInput(const std::string& portName);
    void openOutput(const std::string& portName);

    void appendLog(const std::string& line);

    [[nodiscard]] bool isInputConnected(const std::string& name) const;
    [[nodiscard]] bool isOutputConnected(const std::string& name) const;

    mutable std::mutex stateMutex_;
    std::vector<InputPort> inputs_;
    std::vector<OutputPort> outputs_;

    mutable std::mutex logMutex_;
    std::deque<std::string> logLines_;
    static constexpr std::size_t kMaxLogLines = 64;

    bool running_ = false;
};
