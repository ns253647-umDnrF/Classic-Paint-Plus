#pragma once
// Versioned document preferences, independent of image pixels and application INI.
struct PaintServiceData
{
    bool present=false,hasColor=false;
    COLORREF color=RGB(255,255,255),backdrop=RGB(255,255,255);
};
std::string SerializePaintData(const PaintServiceData& data);
PaintServiceData ParsePaintData(const BYTE* bytes,size_t size);
PaintServiceData ReadPngPaintData(const std::vector<BYTE>& bytes);
bool SetPngPaintData(std::vector<BYTE>& bytes,const PaintServiceData* data);
PaintServiceData ReadWebpPaintData(const std::vector<BYTE>& bytes);
bool SetWebpPaintData(std::vector<BYTE>& bytes,const PaintServiceData& data,int width,int height,bool alpha);
PaintServiceData ReadTgaPaintData(const std::vector<BYTE>& bytes);
bool SetTgaPaintData(std::vector<BYTE>& bytes,const PaintServiceData& data);
bool SupportsPaintData(SaveFormat format);
bool CanSavePaintData();
PaintServiceData CurrentPaintData();
bool PreservePaintData();
void SetPreservePaintData(bool value);
