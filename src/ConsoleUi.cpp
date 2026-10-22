#include "ConsoleUi.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <sstream>

#include <ncurses.h>

namespace
{
enum ColorPairId
{
    kHeader = 1,
    kConnected,
    kIdle,
    kStatusBar,
    kLog
};

std::atomic<bool> gRequestQuit { false };

void onSignal(int /*signal*/)
{
    gRequestQuit.store(true);
}
} // namespace

ConsoleUi::ConsoleUi(MidiRouter& router)
    : router_(router)
{
}

void ConsoleUi::setupColors()
{
    if (!has_colors())
        return;

    start_color();
    use_default_colors();
    init_pair(kHeader, COLOR_CYAN, -1);
    init_pair(kConnected, COLOR_GREEN, -1);
    init_pair(kIdle, COLOR_WHITE, -1);
    init_pair(kStatusBar, COLOR_BLACK, COLOR_WHITE);
    init_pair(kLog, COLOR_YELLOW, -1);
}

void ConsoleUi::truncateToWidth(std::string& text, int maxWidth) const
{
    if (maxWidth <= 0)
    {
        text.clear();
        return;
    }

    if (static_cast<int>(text.size()) <= maxWidth)
        return;

    if (maxWidth <= 1)
    {
        text.resize(static_cast<std::size_t>(maxWidth));
        return;
    }

    text.resize(static_cast<std::size_t>(maxWidth - 3));
    text += "...";
}

void ConsoleUi::drawPortColumn(int x, int width, int y, int height, const char* title,
                               const std::vector<MidiPortView>& ports)
{
    attron(COLOR_PAIR(kHeader));
    mvhline(y, x, ACS_HLINE, width);
    mvprintw(y + 1, x + 1, "%s", title);
    attroff(COLOR_PAIR(kHeader));

    mvvline(y, x, ACS_VLINE, height);
    //mvvline(y, x + width - 1, ACS_VLINE, height);
    mvhline(y + height - 1, x, ACS_HLINE, width);

    const int listY = y + 2;
    const int listHeight = height - 3;

    if (ports.empty())
    {
        attron(COLOR_PAIR(kIdle) | A_DIM);
        mvprintw(listY, x + 2, "(no ports)");
        attroff(COLOR_PAIR(kIdle) | A_DIM);
        return;
    }

    for (int row = 0; row < listHeight; ++row)
    {
        move(listY + row, x + 1);
        clrtoeol();

        if (row >= static_cast<int>(ports.size()))
            continue;

        const auto& port = ports[static_cast<std::size_t>(row)];
        const int nameWidth = width - 4;

        if (port.connected)
            attron(COLOR_PAIR(kConnected) | A_BOLD);
        else
            attron(COLOR_PAIR(kIdle) | A_DIM);

        std::string label = port.connected ? "* " : "o ";
        label += port.name;
        truncateToWidth(label, nameWidth);
        mvprintw(listY + row, x + 2, "%s", label.c_str());

        if (port.connected)
            attroff(COLOR_PAIR(kConnected) | A_BOLD);
        else
            attroff(COLOR_PAIR(kIdle) | A_DIM);
    }
}

void ConsoleUi::drawLogArea(int y, int height, int width)
{
    const auto lines = router_.recentLogLines();
    const int visible = height - 1;
    const int start = static_cast<int>(lines.size()) > visible
                          ? static_cast<int>(lines.size()) - visible
                          : 0;

    attron(COLOR_PAIR(kLog));
    for (int row = 0; row < visible; ++row)
    {
        move(y + row, 0);
        clrtoeol();

        const int index = start + row;
        if (index >= static_cast<int>(lines.size()))
            continue;

        std::string line = lines[static_cast<std::size_t>(index)];
        truncateToWidth(line, width - 1);
        mvprintw(y + row, 0, "%s", line.c_str());
    }
    attroff(COLOR_PAIR(kLog));

    mvhline(y + height - 1, 0, ACS_HLINE, width);
}

void ConsoleUi::draw()
{
    const int rows = getmaxy(stdscr);
    const int cols = getmaxx(stdscr);

    if (rows < 8 || cols < 40)
    {
        erase();
        mvprintw(0, 0, "Terminal too small (need at least 40x8).");
        refresh();
        return;
    }

    erase();

    const int headerRows = 2;
    const int statusRows = 2;
    const int logRows = std::max(5, rows / 4);
    const int portRows = rows - headerRows - statusRows - logRows;

    attron(COLOR_PAIR(kHeader) | A_BOLD);
    mvprintw(0, 0, " midiPipe - all inputs -> all outputs ");
    attroff(COLOR_PAIR(kHeader) | A_BOLD);

    const int columnGap = 1;
    const int columnWidth = (cols - columnGap) / 2;

    const auto inputs = router_.inputPorts();
    const auto outputs = router_.outputPorts();

    std::ostringstream inputTitle;
    inputTitle << "Inputs (" << inputs.size() << ")";
    std::ostringstream outputTitle;
    outputTitle << "Outputs (" << outputs.size() << ")";

    drawPortColumn(0, columnWidth, headerRows, portRows, inputTitle.str().c_str(), inputs);
    drawPortColumn(columnWidth + columnGap, columnWidth, headerRows, portRows,
                   outputTitle.str().c_str(), outputs);

    const int logY = headerRows + portRows;
    drawLogArea(logY, logRows, cols);

    const int statusY = logY + logRows;
    attron(COLOR_PAIR(kStatusBar));
    mvhline(statusY, 0, ' ', cols);
    mvprintw(statusY, 0,
             "q quit  r refresh");
    attroff(COLOR_PAIR(kStatusBar));


    refresh();
}

int ConsoleUi::run()
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    setupColors();

    router_.start();

    int ch = 0;
    while (!gRequestQuit.load())
    {
        router_.refreshPorts();
        draw();

        ch = getch();
        if (ch == 'q' || ch == 'Q')
            break;
        if (ch == 'r' || ch == 'R')
            router_.refreshPorts();
    }

    router_.stop();
    endwin();

    return EXIT_SUCCESS;
}
