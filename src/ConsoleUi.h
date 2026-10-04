#pragma once

#include <string>
#include <vector>

#include "MidiRouter.h"

class ConsoleUi
{
public:
    explicit ConsoleUi(MidiRouter& router);

    ConsoleUi(const ConsoleUi&) = delete;
    ConsoleUi& operator=(const ConsoleUi&) = delete;

    int run();

private:
    void setupColors();
    void draw();
    void drawPortColumn(int x, int width, int y, int height, const char* title,
                        const std::vector<MidiPortView>& ports);
    void drawLogArea(int y, int height, int width);
    void truncateToWidth(std::string& text, int maxWidth) const;

    MidiRouter& router_;
    int refreshIntervalMs_ = 500;
};
