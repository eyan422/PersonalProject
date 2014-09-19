#include "stack.h"

int main(int argc, char *argv[])
{
    int ilRC = RC_SUCCESS;

    /*0 check the input parameter*/
    ilRC = checkInput(argc, argv);
    if (ilRC == RC_FAIL)
    {
        return ilRC;
    }

    /*1 read spreadsheet*/
    readSpreadsheet(argv[1], &rgFile);
    buildArray(rgFile.rlLine[0].pclLine, &rgSpec);
    storeID(&rgFile, rgSpec);

    calculateValue(&rgFile, rgSpec);

    /*
    initArray(rgSpec);
    */

    #if 0
    #endif

    return 0;
}


