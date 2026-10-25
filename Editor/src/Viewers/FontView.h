#pragma once

#include "ViewerManager.h"

#include <filesystem>

class Font;
class FrameBuffer;
class OrthographicCamera;

class FontView :
    public View
{
public:
    FontView(bool* show, std::filesystem::path filepath);
    ~FontView() = default;

    virtual void OnAttach() override;
    virtual void OnImGuiRender() override;

private:
    bool* m_Show;

    std::filesystem::path m_Filepath;

    Ref<Font> m_Font;

    Ref<FrameBuffer> m_Framebuffer;

    Ref<OrthographicCamera> m_Camera;

    const std::string m_String = "The quick brown fox jumps over the lazy dog\n\n!\"#$%&\'()*+,-./0123456789:;<=>?@ ABCDEFGHIJKLMNOPQRSTUVWXYZ [\\]^_`abcdefghijklmnopqrstuvwxyz{ | }~'";
};
