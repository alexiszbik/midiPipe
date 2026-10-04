#include "ConsoleUi.h"
#include "MidiRouter.h"

int main()
{
    MidiRouter router;
    ConsoleUi ui(router);
    return ui.run();
}
