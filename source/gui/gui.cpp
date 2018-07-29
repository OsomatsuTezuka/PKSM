/*
*   This file is part of PKSM
*   Copyright (C) 2016-2018 Bernardo Giordano, Admiral Fish, piepie62
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "gui.hpp"

C3D_RenderTarget* g_renderTargetTop;
C3D_RenderTarget* g_renderTargetBottom;
C2D_TextBuf g_widthBuf;

static C2D_SpriteSheet spritesheet_ui;
static C2D_SpriteSheet spritesheet_pkm;
static C2D_SpriteSheet spritesheet_types;
static C2D_Image bgBoxes;
static C2D_TextBuf dynamicBuf;
static C2D_TextBuf staticBuf;
static std::unordered_map<std::string, C2D_Text> staticMap;

static std::stack<std::unique_ptr<Screen>> screens;

static Tex3DS_SubTexture _select_box(const C2D_Image& image, int x, int y, int dx, int dy)
{
    Tex3DS_SubTexture tex = *image.subtex;
    if (x != dx)
    {
        int deltaX = dx - x;
        float texRL = tex.left - tex.right;
        tex.left = tex.left - (float) texRL / tex.width * x;
        tex.right = tex.left - (float) texRL / tex.width * deltaX;
        tex.width = deltaX;
    }
    if (y != dy)
    {
        float texTB = tex.top - tex.bottom;
        int deltaY = dy - y;
        tex.top = tex.top - (float) texTB / tex.height * y;
        tex.bottom = tex.top - (float) texTB / tex.height * deltaY;
        tex.height = deltaY;
    }
    return tex;
}

C2D_Image Gui::TWLIcon(void)
{
    return C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_gameselector_twlcart_idx);
}

void Gui::backgroundBottom(void)
{
    C2D_DrawRectSolid(0, 0, 0.5f, 320, 240, COLOR_PALEBLUE);
    C2D_DrawRectSolid(0, 221, 0.5f, 320, 19, COLOR_MENUBLUE);
}

void Gui::backgroundTop(void)
{
    C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, COLOR_MENUBLUE);
    C2D_DrawRectSolid(0, 0, 0.5f, 400, 19, COLOR_HIGHBLUE);
}

void Gui::backgroundAnimated(gfxScreen_t screen)
{
    static bool firstRun = true;
    static int boxesX[11] = {0};

    int maxrange = screen == GFX_TOP ? 400 : 320;

    if (firstRun)
    {
        firstRun = false;
        for (int i = 0; i < 11; i++)
        {
            boxesX[i] = (i - 1) * (maxrange / 10);
        }
    }

    Tex3DS_SubTexture boxesPart; // = _select_box(bgBoxes, maxrange - boxesX / 2, 0, 800, 240);
    for (u8 i = 0; i < 11; i++)
    {
        boxesPart = _select_box(bgBoxes, i * (maxrange / 10), 0, (i + 1) * (maxrange / 10), 240);
        C2D_DrawImageAt({bgBoxes.tex, &boxesPart}, boxesX[i], 0, 0.5f);
        boxesX[i] = boxesX[i] >= maxrange ? - maxrange / 10 : boxesX[i] + 1;
    }
}

void Gui::clearTextBufs(void)
{
    C2D_TextBufClear(dynamicBuf);
    C2D_TextBufClear(g_widthBuf);
}

void Gui::dynamicText(const std::string& str, int x, int y, float scaleX, float scaleY, u32 color, bool rightAligned)
{
    C2D_Text text;
    C2D_TextParse(&text, dynamicBuf, str.c_str());
    C2D_TextOptimize(&text);
    if (rightAligned)
    {
        x = x - textWidth(text, scaleX);
    }
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

// TODO: move to StringUtils
std::string splitWord(std::string& word, float scaleX, float maxWidth)
{
    float defaultWidth = scaleX * fontGetInfo()->defaultWidth.charWidth;
    if (textWidth(word, scaleX) > maxWidth)
    {
        // TODO: maxChars is wrong:
        // with lowercase, it works with size_t maxChars = ceilf(maxWidth / defaultWidth) * 2;
        // with uppercase, it works with ceilf(maxWidth / defaultWidth * 1.25f);
        size_t maxChars = ceilf(maxWidth / defaultWidth);
        for (std::string::iterator it = word.begin() + maxChars; it < word.end(); it += maxChars)
        {
            word.insert(it++, '\n');
        }
    }
    return word;
}

void Gui::dynamicText(const std::string& str, int x, int y, float scaleX, float scaleY, u32 color, float maxWidth)
{
    std::string dst, line, word;
    dst = line = word = "";

    for (std::string::const_iterator it = str.begin(); it != str.end(); it++)
    {
        word += *it;
        if (*it == ' ')
        {
            // split single words that are bigger than maxWidth
            word = splitWord(word, scaleX, maxWidth);
            if (textWidth(line + word, scaleX) <= maxWidth)
            {
                line += word;
            }
            else
            {
                dst += line + '\n';
                line = word;
            }
            word = "";
        }
    }

    // attach line leftovers to word
    word = line + word;

    // we're out of the loop, what if the last word is longer than maxWidth? split it
    dst += splitWord(word, scaleX, maxWidth);

    C2D_Text text;
    C2D_TextParse(&text, dynamicBuf, dst.c_str());
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

void Gui::dynamicText(gfxScreen_t screen, int y, const std::string& text, float scaleX, float scaleY, u32 color)
{
    float width = (screen == GFX_TOP) ? 400.0f : 320.0f;
    int x = ceilf((width - textWidth(text, scaleX)) / 2.0f);
    dynamicText(text, x, y, scaleX, scaleY, color);
}

void Gui::dynamicText(int x, int y, float width, const std::string& text, float scaleX, float scaleY, u32 color)
{
    int drawX = x + ceilf((width - textWidth(text, scaleX)) / 2.0f);
    dynamicText(text, drawX, y, scaleX, scaleY, color);
}

C2D_Text Gui::cacheStaticText(const std::string& strKey)
{
    C2D_Text text;
    std::unordered_map<std::string, C2D_Text>::const_iterator index = staticMap.find(strKey);
    if (index == staticMap.end())
    {
        C2D_TextParse(&text, staticBuf, strKey.c_str());
        C2D_TextOptimize(&text);
        staticMap.emplace(strKey, text);
    }
    else
    {
        return index->second;
    }

    return text;
}

void Gui::staticText(const std::string& strKey, int x, int y, float scaleX, float scaleY, u32 color, bool rightAligned)
{
    C2D_Text text = cacheStaticText(strKey);
    if (rightAligned)
    {
        x = x - textWidth(text, scaleX);
    }
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

void Gui::staticText(const std::string& strKey, int x, int y, float scaleX, float scaleY, u32 color, float maxWidth)
{
    std::string print = strKey;
    C2D_Text text;
    std::unordered_map<std::string, C2D_Text>::const_iterator index = staticMap.find(strKey);
    if (index == staticMap.end())
    {
        float defaultWidth = scaleX * fontGetInfo()->defaultWidth.charWidth;
        float width = defaultWidth * print.length();
        if (width > maxWidth)
        {
            size_t maxChars = ceilf(maxWidth / defaultWidth);
            for (std::string::iterator it = print.begin() + maxChars; it < print.end(); it += maxChars)
            {
                print.insert(it++, '\n');
            }
        }
        C2D_TextParse(&text, dynamicBuf, print.c_str());
        C2D_TextOptimize(&text);
        staticMap.emplace(strKey, text);
    }
    else
    {
        text = index->second;
    }
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

void Gui::staticText(gfxScreen_t screen, int y, const std::string& strKey, float scaleX, float scaleY, u32 color)
{
    C2D_Text text = cacheStaticText(strKey);
    float width = (screen == GFX_TOP) ? 400.0f : 320.0f;
    int x = ceilf((width - textWidth(text, scaleX)) / 2.0f);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

void Gui::staticText(int x, int y, float width, const std::string& strKey, float scaleX, float scaleY, u32 color)
{
    C2D_Text text = cacheStaticText(strKey);
    int drawX = x + ceilf((width - textWidth(text, scaleX)) / 2.0f);
    C2D_DrawText(&text, C2D_WithColor, drawX, y, 0.5f, scaleX, scaleY, color);
}

static void _draw_mirror_scale(int key, int x, int y, int off, int rep)
{
    C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
    // Left side
    Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
    C2D_DrawImageAt({sprite.tex, &tex}, x, y, 0.5f);
    // Right side
    float right = tex.right;
    tex.right = tex.left;
    tex.left = right;
    C2D_DrawImageAt({sprite.tex, &tex}, x + off + rep, y, 0.5f);
    // Center
    tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
    C2D_DrawImageAt({sprite.tex, &tex}, x + off, y, 0.5f, nullptr, rep, 1.0f);
}

static void _draw_repeat(int key, int x, int y, u8 rows, u8 cols)
{
    C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
    for (u8 row = 0; row < rows; row++)
    {
        for (u8 col = 0; col < cols; col++)
        {
            C2D_DrawImageAt(sprite, x + col * sprite.subtex->width, y + row * sprite.subtex->height, 0.5f);
        }
    }
}

Result Gui::init(void)
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    srand(time(NULL));

    g_renderTargetTop = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_renderTargetBottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    dynamicBuf = C2D_TextBufNew(1024);
    g_widthBuf = C2D_TextBufNew(1024);
    staticBuf = C2D_TextBufNew(1024);

    spritesheet_ui = C2D_SpriteSheetLoad("romfs:/gfx/ui_sheet.t3x");
    spritesheet_pkm = C2D_SpriteSheetLoad("romfs:/gfx/pkm_spritesheet.t3x");
    spritesheet_types = C2D_SpriteSheetLoad("romfs:/gfx/types_spritesheet.t3x");

    bgBoxes = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_anim_squares_idx);

    return 0;
}

void Gui::mainLoop(void)
{
    bool exit = false;
    while (aptMainLoop() && !exit)
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(g_renderTargetTop, COLOR_BLACK);
        C2D_TargetClear(g_renderTargetBottom, COLOR_BLACK);

        screens.top()->draw();
        touchPosition touch;
        hidTouchRead(&touch);
        screens.top()->update(&touch);
        exit = screens.top()->type() == ScreenType::MAINMENU && (hidKeysDown() & KEY_START);

        C3D_FrameEnd(0);
        Gui::clearTextBufs();
    }
}

void Gui::exit(void)
{
    C2D_SpriteSheetFree(spritesheet_ui);
    C2D_SpriteSheetFree(spritesheet_pkm);
    C2D_SpriteSheetFree(spritesheet_types);
    C2D_TextBufDelete(g_widthBuf);
    C2D_TextBufDelete(dynamicBuf);
    C2D_TextBufDelete(staticBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

void Gui::sprite(int key, int x, int y)
{
    if (key == ui_sheet_res_null_idx)
    {
        return;
    }
    else if (key == ui_sheet_eventmenu_bar_selected_idx || key == ui_sheet_eventmenu_bar_unselected_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        // Left side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        C2D_DrawImageAt({sprite.tex, &tex}, x, y, 0.5f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        C2D_DrawImageAt({sprite.tex, &tex}, x + off, y, 0.5f, nullptr, rep, 1.0f);
    }
    else if (key == ui_sheet_eventmenu_page_indicator_idx)
    {
        _draw_mirror_scale(key, x, y, 4, 182);
    }
    else if (key == ui_sheet_gameselector_bg_idx)
    {
        u8 off = 5, rep = 197;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        // Top side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 0, off);
        C2D_DrawImageAt({sprite.tex, &tex}, x, y, 0.5f);
        // Bottom side
        float top = tex.top;
        tex.top = tex.bottom;
        tex.bottom = top;
        C2D_DrawImageAt({sprite.tex, &tex}, x, y + off + rep, 0.5f);
        // Center
        tex = _select_box(sprite, 0, off, 0, sprite.subtex->height);
        C2D_DrawImageAt({sprite.tex, &tex}, x, y + off, 0.5f, nullptr, 1.0f, rep);       
    }
    else if (key == ui_sheet_gameselector_savebox_idx)
    {
        u8 off = 53;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        // Top side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 0, off);
        C2D_DrawImageAt({sprite.tex, &tex}, x, y, 0.5f);
        // Bottom side
        float top = tex.top;
        tex.top = tex.bottom;
        tex.bottom = top;
        C2D_DrawImageAt({sprite.tex, &tex}, x, y + off - 1, 0.5f);         
    }
    else if (key == ui_sheet_mainmenu_button_idx)
    {
        _draw_mirror_scale(key, x, y, 5, 132);
    }
    else if (key == ui_sheet_part_editor_20x2_idx)
    {
        _draw_repeat(key, x, y, 20, 1);
    }
    else if (key == ui_sheet_part_info_bottom_idx || key == ui_sheet_part_info_top_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        int width = sprite.subtex->width, height = sprite.subtex->height;
        // Top left
        C2D_DrawImageAt(sprite, x, y, 0.5f);
        // Top right
        C2D_DrawImageAt(sprite, x + width, y, 0.5f, NULL, -1.0f, 1.0f);
        // Bottom left
        C2D_DrawImageAt(sprite, x, y + height, 0.5f, NULL, 1.0f, -1.0f);
        // Bottom right
        C2D_DrawImageAt(sprite, x + width, y + height, 0.5f, NULL, -1.0f, -1.0f);
    }
    else if (key == ui_sheet_part_mtx_4x4_idx)
    {
        _draw_repeat(key, x, y, 4, 4);
    }
    else if (key == ui_sheet_part_mtx_5x6_idx)
    {
        _draw_repeat(key, x, y, 5, 6);
    }
    else if (key == ui_sheet_part_mtx_5x8_idx)
    {
        _draw_repeat(key, x, y, 5, 8);
    }
    else if (key == ui_sheet_part_mtx_15x16_idx)
    {
        _draw_repeat(key, x, y, 15, 16);
        C2D_DrawRectSolid(0, 225, 0.5f, 400, 15, COLOR_WHITE);
    }
    
    //emulated
    else if (key == ui_sheet_emulated_pointer_horizontal_flipped_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_pointer_horizontal_idx);
        C2D_DrawImageAt(sprite, x, y, 0.5f, NULL, -1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_bg_top_red)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(228, 143, 131, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(201, 95, 84, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(239, 163, 151, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(214, 117, 106, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_bg_top_blue)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(139, 171, 221, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(93, 134, 193, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(158, 186, 233, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(113, 150, 205, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_bg_top_green)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(142, 221, 138, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(101, 193, 93, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(161, 233, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(119, 205, 113, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_bg_bottom_red)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(216, 122, 111, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(239, 163, 151, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(201, 95, 84, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(224, 134, 123, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, 0.5f, &tint);        
    }
    else if (key == ui_sheet_emulated_bg_bottom_blue)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(120, 154, 209, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(158, 186, 233, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(93, 134, 193, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(131, 165, 217, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, 0.5f, &tint);  
    }
    else if (key == ui_sheet_emulated_bg_bottom_green)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(125, 209, 119, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(161, 233, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(101, 193, 93, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(136, 217, 131, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, 0.5f, &tint);        
    }
    // TODO: flipped event menu bar
    
    // standard case
    else
    {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, key), x, y, 0.5f);
    }
}

void Gui::generation(PKX* pkm, int x, int y)
{
    switch (pkm->version())
    {
        case 1: // sapphire
		case 2: // ruby
		case 3: // emerald
		case 4: // fire red
		case 5: // leaf green
		case 15: // colosseum/XD
			Gui::sprite(ui_sheet_icon_generation_3_idx, x, y);
            break;
		case 10: // diamond
		case 11: // pearl
		case 12: // platinum
		case 7: // heart gold
		case 8: // soul silver
			Gui::sprite(ui_sheet_icon_generation_4_idx, x, y);
            break;		
		case 20: // white
		case 21: // black
		case 22: // white2
		case 23: // black2
			Gui::sprite(ui_sheet_icon_generation_5_idx, x, y);
            break;
		case 24: // x
		case 25: // y
		case 26: // as
		case 27: // or
			Gui::sprite(ui_sheet_icon_generation_6_idx, x, y);
            break;
		case 30: // sun
		case 31: // moon
		case 32: // us
		case 33: // um
			Gui::sprite(ui_sheet_icon_generation_7_idx, x, y);
            break;
		case 34: // go
			Gui::sprite(ui_sheet_icon_generation_go_idx, x, y);
            break;
		case 35: // rd
		case 36: // gn
		case 37: // bu
		case 38: // yw
		case 39: // gd
		case 40: // sv
			Gui::sprite(ui_sheet_icon_generation_gb_idx, x, y);
            break;
		default:
			break;
    }
}

void Gui::sprite(int key, int x, int y, u32 color)
{
    if (key == ui_sheet_button_editor_idx
       || key == ui_sheet_icon_item_idx
       || key == ui_sheet_pointer_arrow_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        C2D_ImageTint tint;
        for (int i = 0; i < 4; i++)
        {
            tint.corners[i] = {color, 1.0f};
        }
        C2D_DrawImageAt(sprite, x, y, 0.5f, &tint);
    }
}

void Gui::pkm(PKX* pokemon, int x, int y, u32 color, float blend)
{
    if (pokemon == NULL)
    {
        return;
    }
    static C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);

    if (pokemon->egg())
    {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_0_idx), x, y, 0.5f, &tint);
    }
    else
    {
        if (pokemon->alternativeForm() == 0 || pokemon->species() != 201)
        {
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pokemon->formSpecies()), x, y, 0.5f, &tint);
        }
        else
        {
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_744_1_idx + pokemon->alternativeForm()), x, y, 0.5f, &tint);
        }
        if (pokemon->heldItem() > 0)
        {
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_icon_item_idx), x + 3, y + 21, 0.5f, &tint);
        }
    }
}

void Gui::pkm(int formSpecies, int x, int y, u32 color, float blend)
{
    static C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);
    C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, formSpecies), x, y, 0.5f, &tint);
}

void Gui::ball(size_t index, int x, int y)
{
    if (index < 27)
    {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, index + pkm_spritesheet_empty_idx), x, y, 0.5f);
    }
    else
    {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_empty_idx), x, y, 0.5f);
    }
}

/*void Gui::eventList(WCX* database[], int currentEntry, int page) // This database could be an STL class, too
{
    C2D_SceneBegin(g_renderTargetTop);
    printMenuTop();
    // do title text
    int y = 41;
    std::string pages = std::to_string(page + 1) + "/" + "WHEE"; // + mysteryGiftTotal() / 10;
    C2D_TextBufClear(dynamicBuf);
    for (int i = 0; i < 5; i++)
    {
        if (i == currentEntry)
        {
            sprite(ui_sheet_res_bar_event_top_selected_idx, 18, y);
        }
        else
        {
            sprite(ui_sheet_res_bar_event_top_normal_idx, 18, y);
        }

        if (database[page * 10 + i]->pokemon()) // Or just use the value?
        {
            if (database[page * 10 + i]->alternativeForm() == 0 || database[page * 10 + i]->species() != 201)
            {
                pkm(database[page * 10 + i]->formSpecies(), 20, y - ((i == currentEntry) ? movementOffsetSlow(2) : 0));
            }
            else
            {
                pkm(pkm_spritesheet_744_1_idx + database[page * 10 + i]->alternativeForm(), 20, y - ((i == currentEntry) ? movementOffsetSlow(2) : 0));
            }
        }

        float textWidth = _text_width(database[page * 10 + i]->title(), FONT_SIZE_9);
        if (textWidth < 140.0f)
        {
            _draw_text(54, y + 12, database[page * 10 + i]->title(), (i == currentEntry) ? COLOR_HIGHBLUE : COLOR_YELLOW, FONT_SIZE_9, FONT_SIZE_9);
        }
        else
        {
            _draw_text_wrap(54, y + 3, database[page * 10 + i]->title(), 140.0f, (i == currentEntry) ? COLOR_HIGHBLUE : COLOR_YELLOW, FONT_SIZE_9, FONT_SIZE_9);
        }
        y += 37;
    }

    y = 41;
    for (int i = 5; i < 10; i++)
    {
        if (i == currentEntry)
        {
            sprite(ui_sheet_res_bar_event_top_selected_idx, 200, y);
        }
        else
        {
            sprite(ui_sheet_res_bar_event_top_normal_idx, 200, y);
        }

        if (database[page * 10 + i]->pokemon()) // Or just use the value?
        {
            if (database[page * 10 + i]->alternativeForm() == 0 || database[page * 10 + i]->species() != 201)
            {
                pkm(database[page * 10 + i]->formSpecies(), 20, y - ((i == currentEntry) ? movementOffsetSlow(2) : 0));
            }
            else
            {
                pkm(pkm_spritesheet_744_1_idx + database[page * 10 + i]->alternativeForm(), 20, y - ((i == currentEntry) ? movementOffsetSlow(2) : 0));
            }
        }

        float textWidth = _text_width(database[page * 10 + i]->title(), FONT_SIZE_9);
        if (textWidth < 140.0f)
        {
            _draw_text(235, y + 14, database[page * 10 + i]->title(), (i == currentEntry) ? COLOR_HIGHBLUE : COLOR_YELLOW, FONT_SIZE_9, FONT_SIZE_9);
        }
        else
        {
            _draw_text_wrap(235, y + 3, database[page * 10 + i]->title(), 140.0f, (i == currentEntry) ? COLOR_HIGHBLUE : COLOR_YELLOW, FONT_SIZE_9, FONT_SIZE_9);
        }
        y += 37;
    }
    
    C2D_SceneBegin(g_renderTargetBottom);
    printMenuBottom();
    sprite(ui_sheet_res_bar_event_bottom_idx, 65, 45);
    C2D_DrawText(&LButton, C2D_WithColor, 83, 52, 0.5f, FONT_SIZE_12, FONT_SIZE_12, COLOR_WHITE);
    C2D_DrawText(&RButton, C2D_WithColor, 221, 52, 0.5f, FONT_SIZE_12, FONT_SIZE_12, COLOR_WHITE);
    _draw_text_center(g_renderTargetBottom, 52, pages);
}*/

static C2D_Image typeImage(Language lang, u8 type)
{
    switch (lang)
    {
        case Language::ES:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_es_00_idx + type);
        case Language::DE:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_de_00_idx + type);
        case Language::FR:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_fr_00_idx + type);
        case Language::IT:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_it_00_idx + type);
        case Language::JP:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_jp_00_idx + type);
        case Language::KO:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_ko_00_idx + type);
        case Language::TW:
        case Language::ZH:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_zh_00_idx + type);
        case Language::EN:
        case Language::PT:
        case Language::NL:
        default:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_en_00_idx + type);
    }
}

void Gui::type(Language lang, u8 type, int x, int y)
{
    C2D_DrawImageAt(typeImage(lang, type), x, y, 0.5f);
}

void Gui::setScreen(std::unique_ptr<Screen> screen)
{
    if (!screens.empty() && screens.top()->type() == screen->type())
    {
        if (screen == screens.top())
            return;
    }
    screens.push(std::move(screen));
}

u8 transparencyWaver()
{
    static u8 currentAmount = 255;
    static bool dir = true;
    if (!dir)
    {
        currentAmount++;
        if (currentAmount == 255)
            dir = true;
    }
    else
    {
        currentAmount--;
        if (currentAmount < 155)
            dir = false;
    }
    return currentAmount;
}

bool Gui::showChoiceMessage(const std::string& message)
{
    u32 keys = 0;
    while (aptMainLoop() && !((keys = hidKeysHeld()) & KEY_B))
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(g_renderTargetTop, COLOR_BLACK);
        C2D_TargetClear(g_renderTargetBottom, COLOR_BLACK);

        C2D_SceneBegin(g_renderTargetTop);
        sprite(ui_sheet_part_info_top_idx, 0, 0);
        dynamicText(GFX_TOP, 95, message, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparencyWaver()));

        dynamicText(GFX_TOP, 130, "Press A to continue, B to cancel.", FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE);

        C2D_SceneBegin(g_renderTargetBottom);
        sprite(ui_sheet_part_info_bottom_idx, 0, 0);

        C3D_FrameEnd(0);
        Gui::clearTextBufs();
        if (keys & KEY_A)
        {
            hidScanInput();
            return true;
        }
    }
    hidScanInput();
    return false;
}

void Gui::screenBack()
{
    screens.pop();
}