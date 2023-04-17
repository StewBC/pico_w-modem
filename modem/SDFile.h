#include "Stream.h"
#include "sd_card.h"
#include "ff.h"

class SDFile_ : public Stream
{
private:
    FRESULT fr;
    FATFS fs;
    FIL fil;

public:
    SDFile_()    {;}
    ~SDFile_()   {;}

    FRESULT begin();
    FRESULT open(const TCHAR* filename, BYTE opt);
    FRESULT close();
    FRESULT seek(size_t offset);
    size_t  Write(const uint8_t* buffer, size_t size);
    size_t  Write(uint8_t c) {return Write(&c,1);}
    int     Read(uint8_t* buffer, size_t size);
    int     Read();
};
