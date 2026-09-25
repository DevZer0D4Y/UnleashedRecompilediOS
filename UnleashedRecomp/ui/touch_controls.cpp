#include "touch_controls.h"
#include "installer_wizard.h"
#include "imgui_utils.h"
#include <gpu/imgui/imgui_snapshot.h>
#include <hid/hid.h>
#include <user/config.h>
#include <app.h>
#include <ui/game_window.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include "touch_haptics_ios.h"
#define TOUCH_CONTROLS_SUPPORTED 1
#else
#define TOUCH_CONTROLS_SUPPORTED 0
#endif

// Layout frames and tuning values match the XeniOS defaults (https://github.com/xenios-jp/XeniOS,
// src/xenia/hid/touch). Xenia is Copyright (c) 2015 Ben Vanik, released under the BSD license.
static constexpr float STICK_DEADZONE = 0.14f;
static constexpr float STICK_ACTIVATION_RADIUS = 0.48f;
static constexpr float STICK_DPAD_RING_RADIUS = 0.32f;
static constexpr float LOOK_POINTS_PER_FULL_SCALE = 4.0f;
static constexpr float LOOK_VERTICAL_SCALE = 1.2f;
static constexpr double LOOK_HOLD_SECONDS = 0.10;
static constexpr double BUTTON_TAP_HOLD_SECONDS = 0.10;

// Horizontal margin kept free on wide screens so controls stay clear of the notch and rounded corners.
static constexpr float WIDE_SCREEN_SAFE_MARGIN = 0.055f;

struct TouchRect
{
    float x, y, width, height;
};

enum class TouchControlKind
{
    Stick,
    Button
};

struct TouchControl
{
    TouchControlKind kind;
    const char* label;
    TouchRect frame; // Normalised to the layout space.
    uint16_t buttons;
    uint8_t leftTrigger;
    uint8_t rightTrigger;
};

// Layout frames come from the XeniOS default layout, with its actions mapped to their gamepad equivalents.
static const TouchControl g_controls[] =
{
    { TouchControlKind::Stick,  nullptr, { 0.055f, 0.560f, 0.190f, 0.315f }, 0, 0, 0 },
    { TouchControlKind::Button, "BACK",  { 0.390f, 0.045f, 0.080f, 0.112f }, XAMINPUT_GAMEPAD_BACK, 0, 0 },
    { TouchControlKind::Button, "START", { 0.495f, 0.045f, 0.085f, 0.112f }, XAMINPUT_GAMEPAD_START, 0, 0 },
    { TouchControlKind::Button, "LB",    { 0.660f, 0.050f, 0.085f, 0.112f }, XAMINPUT_GAMEPAD_LEFT_SHOULDER, 0, 0 },
    { TouchControlKind::Button, "RB",    { 0.765f, 0.050f, 0.085f, 0.112f }, XAMINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0 },
    { TouchControlKind::Button, "LT",    { 0.095f, 0.405f, 0.120f, 0.110f }, 0, 0xFF, 0 },
    { TouchControlKind::Button, "RT",    { 0.860f, 0.405f, 0.120f, 0.110f }, 0, 0, 0xFF },
    { TouchControlKind::Button, "Y",     { 0.760f, 0.455f, 0.065f, 0.115f }, XAMINPUT_GAMEPAD_Y, 0, 0 },
    { TouchControlKind::Button, "X",     { 0.700f, 0.585f, 0.065f, 0.115f }, XAMINPUT_GAMEPAD_X, 0, 0 },
    { TouchControlKind::Button, "B",     { 0.820f, 0.585f, 0.065f, 0.115f }, XAMINPUT_GAMEPAD_B, 0, 0 },
    { TouchControlKind::Button, "A",     { 0.760f, 0.715f, 0.065f, 0.115f }, XAMINPUT_GAMEPAD_A, 0, 0 },
};

static constexpr size_t CONTROL_COUNT = std::size(g_controls);
static constexpr int LOOK_ZONE = -1;

enum class StickZone
{
    Stick,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight
};

struct TouchCapture
{
    SDL_FingerID fingerId;
    int control;
    StickZone stickZone;
    ImVec2 anchor;  // Window points.
    ImVec2 current; // Window points.
};

static std::mutex g_mutex;
static std::vector<TouchCapture> g_captures;
static double g_buttonPressTimes[CONTROL_COUNT];
static ImVec2 g_lookVector;
static double g_lookTime = -1.0;
static ImVec2 g_windowSize{ 1.0f, 1.0f };
static std::atomic<bool> g_hiddenByController;
static ImFont* g_font;

static double GetTime()
{
    return double(SDL_GetPerformanceCounter()) / double(SDL_GetPerformanceFrequency());
}

static bool IsEnabled()
{
#if TOUCH_CONTROLS_SUPPORTED
    return Config::TouchControls && !InstallerWizard::s_isVisible;
#else
    return false;
#endif
}

static TouchRect GetLayoutSpace(ImVec2 size)
{
    float margin = (size.x / size.y) > 1.9f ? size.x * WIDE_SCREEN_SAFE_MARGIN : 0.0f;
    return { margin, 0.0f, size.x - margin * 2.0f, size.y };
}

// Resolves a control's frame in window points.
static TouchRect GetControlFrame(const TouchControl& control, ImVec2 size)
{
    TouchRect space = GetLayoutSpace(size);

    return
    {
        space.x + control.frame.x * space.width,
        space.y + control.frame.y * space.height,
        control.frame.width * space.width,
        control.frame.height * space.height
    };
}

static ImVec2 GetCentre(const TouchRect& rect)
{
    return { rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f };
}

static float GetRadius(const TouchRect& rect)
{
    return std::min(rect.width, rect.height) * 0.5f;
}

static bool ControlContainsPoint(const TouchControl& control, const TouchRect& frame, ImVec2 point)
{
    if (control.kind == TouchControlKind::Stick)
    {
        return point.x >= frame.x && point.x <= frame.x + frame.width &&
            point.y >= frame.y && point.y <= frame.y + frame.height;
    }

    // Buttons are pills: circles, stretched along their longer side.
    float radius = GetRadius(frame);
    ImVec2 centre = GetCentre(frame);
    float halfSpanX = std::max(frame.width * 0.5f - radius, 0.0f);
    float halfSpanY = std::max(frame.height * 0.5f - radius, 0.0f);
    float dx = std::max(std::abs(point.x - centre.x) - halfSpanX, 0.0f);
    float dy = std::max(std::abs(point.y - centre.y) - halfSpanY, 0.0f);
    return dx * dx + dy * dy <= radius * radius;
}

static StickZone GetStickZone(const TouchRect& frame, ImVec2 point)
{
    ImVec2 centre = GetCentre(frame);
    float stickRadius = std::min(frame.width, frame.height) * STICK_DPAD_RING_RADIUS;
    float dx = point.x - centre.x;
    float dy = point.y - centre.y;

    if (dx * dx + dy * dy <= stickRadius * stickRadius)
        return StickZone::Stick;

    if (std::abs(dx) > std::abs(dy))
        return dx > 0.0f ? StickZone::DpadRight : StickZone::DpadLeft;

    return dy > 0.0f ? StickZone::DpadDown : StickZone::DpadUp;
}

// Returns the stick deflection for a capture, with Y pointing down.
static ImVec2 GetStickVector(const TouchRect& frame, const TouchCapture& capture)
{
    float outerRadius = std::min(frame.width, frame.height) * STICK_ACTIVATION_RADIUS;
    ImVec2 delta = { capture.current.x - capture.anchor.x, capture.current.y - capture.anchor.y };
    float distance = std::hypot(delta.x, delta.y);

    if (distance > outerRadius && distance > 0.0f)
    {
        delta.x *= outerRadius / distance;
        delta.y *= outerRadius / distance;
    }

    ImVec2 normalised = { delta.x / outerRadius, delta.y / outerRadius };
    float magnitude = std::hypot(normalised.x, normalised.y);

    if (magnitude < STICK_DEADZONE || magnitude <= 0.0f)
        return {};

    float rescaled = std::clamp((magnitude - STICK_DEADZONE) / (1.0f - STICK_DEADZONE), 0.0f, 1.0f);
    return { normalised.x / magnitude * rescaled, normalised.y / magnitude * rescaled };
}

static int16_t ToAxis(float value)
{
    return int16_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
}

static void MergeAxis(int16_t& target, int16_t value)
{
    if (std::abs(int(value)) > std::abs(int(target)))
        target = value;
}

static int FindControlAtPoint(ImVec2 point)
{
    // Buttons take priority over the stick, and the stick over the look zone.
    for (size_t i = 0; i < CONTROL_COUNT; i++)
    {
        if (g_controls[i].kind == TouchControlKind::Button && ControlContainsPoint(g_controls[i], GetControlFrame(g_controls[i], g_windowSize), point))
            return int(i);
    }

    for (size_t i = 0; i < CONTROL_COUNT; i++)
    {
        if (g_controls[i].kind == TouchControlKind::Stick && ControlContainsPoint(g_controls[i], GetControlFrame(g_controls[i], g_windowSize), point))
            return int(i);
    }

    return LOOK_ZONE;
}

static void PlayHaptic(bool light)
{
#if TOUCH_CONTROLS_SUPPORTED
    TouchHaptics::Play(light);
#endif
}

static int TouchControls_OnSDLEvent(void*, SDL_Event* event)
{
    if (event->type != SDL_FINGERDOWN && event->type != SDL_FINGERMOTION && event->type != SDL_FINGERUP)
        return 0;

    std::lock_guard lock(g_mutex);

    if (!IsEnabled())
    {
        g_captures.clear();
        return 0;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(GameWindow::s_pWindow, &windowWidth, &windowHeight);

    if (windowWidth > 0 && windowHeight > 0)
        g_windowSize = { float(windowWidth), float(windowHeight) };

    ImVec2 point = { event->tfinger.x * g_windowSize.x, event->tfinger.y * g_windowSize.y };
    auto capture = std::find_if(g_captures.begin(), g_captures.end(), [&](const TouchCapture& c) { return c.fingerId == event->tfinger.fingerId; });

    switch (event->type)
    {
        case SDL_FINGERDOWN:
        {
            g_hiddenByController = false;

            if (!App::s_isLoading)
            {
                // Show Xbox button prompts, matching the on-screen labels.
                hid::g_inputDevice = hid::EInputDevice::Xbox;
                hid::g_inputDeviceController = hid::EInputDevice::Xbox;
            }

            if (capture != g_captures.end())
                g_captures.erase(capture);

            TouchCapture newCapture{};
            newCapture.fingerId = event->tfinger.fingerId;
            newCapture.control = FindControlAtPoint(point);
            newCapture.anchor = point;
            newCapture.current = point;

            if (newCapture.control != LOOK_ZONE)
            {
                const TouchControl& control = g_controls[newCapture.control];

                if (control.kind == TouchControlKind::Stick)
                {
                    newCapture.stickZone = GetStickZone(GetControlFrame(control, g_windowSize), point);
                    PlayHaptic(true);
                }
                else
                {
                    g_buttonPressTimes[newCapture.control] = GetTime();
                    PlayHaptic(false);
                }
            }

            g_captures.push_back(newCapture);
            break;
        }

        case SDL_FINGERMOTION:
        {
            if (capture == g_captures.end())
                break;

            if (capture->control == LOOK_ZONE)
            {
                float deltaX = event->tfinger.dx * g_windowSize.x;
                float deltaY = event->tfinger.dy * g_windowSize.y;

                g_lookVector =
                {
                    std::clamp(deltaX / LOOK_POINTS_PER_FULL_SCALE, -1.0f, 1.0f),
                    std::clamp(-deltaY / LOOK_POINTS_PER_FULL_SCALE * LOOK_VERTICAL_SCALE, -1.0f, 1.0f)
                };

                g_lookTime = GetTime();
            }

            capture->current = point;
            break;
        }

        case SDL_FINGERUP:
        {
            if (capture != g_captures.end())
                g_captures.erase(capture);

            break;
        }
    }

    return 0;
}

void TouchControls::Init()
{
    g_font = ImFontAtlasSnapshot::GetFont("FOT-NewRodinPro-M.otf");

#if TOUCH_CONTROLS_SUPPORTED
    TouchHaptics::Init();
    SDL_AddEventWatch(TouchControls_OnSDLEvent, nullptr);
#endif
}

bool TouchControls::IsActive()
{
    return IsEnabled() && !g_hiddenByController;
}

void TouchControls::OnPhysicalControllerInput()
{
    std::lock_guard lock(g_mutex);

    g_hiddenByController = true;
    g_captures.clear();
    g_lookTime = -1.0;
}

void TouchControls::Apply(XAMINPUT_GAMEPAD& pad)
{
    if (!IsActive())
        return;

    std::lock_guard lock(g_mutex);

    double time = GetTime();
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
    int16_t leftX = 0;
    int16_t leftY = 0;
    int16_t rightX = 0;
    int16_t rightY = 0;

    for (auto& capture : g_captures)
    {
        if (capture.control == LOOK_ZONE)
            continue;

        const TouchControl& control = g_controls[capture.control];
        TouchRect frame = GetControlFrame(control, g_windowSize);

        if (control.kind == TouchControlKind::Stick)
        {
            switch (capture.stickZone)
            {
                case StickZone::DpadUp:    buttons |= XAMINPUT_GAMEPAD_DPAD_UP; break;
                case StickZone::DpadDown:  buttons |= XAMINPUT_GAMEPAD_DPAD_DOWN; break;
                case StickZone::DpadLeft:  buttons |= XAMINPUT_GAMEPAD_DPAD_LEFT; break;
                case StickZone::DpadRight: buttons |= XAMINPUT_GAMEPAD_DPAD_RIGHT; break;

                case StickZone::Stick:
                {
                    ImVec2 vector = GetStickVector(frame, capture);
                    leftX = ToAxis(vector.x);
                    leftY = ToAxis(-vector.y);
                    break;
                }
            }
        }
        else if (ControlContainsPoint(control, frame, capture.current))
        {
            buttons |= control.buttons;
            leftTrigger = std::max(leftTrigger, control.leftTrigger);
            rightTrigger = std::max(rightTrigger, control.rightTrigger);
        }
    }

    // Keep quick taps pressed for long enough that the game sees them even at low frame rates.
    for (size_t i = 0; i < CONTROL_COUNT; i++)
    {
        if (g_controls[i].kind == TouchControlKind::Button && (time - g_buttonPressTimes[i]) < BUTTON_TAP_HOLD_SECONDS)
        {
            buttons |= g_controls[i].buttons;
            leftTrigger = std::max(leftTrigger, g_controls[i].leftTrigger);
            rightTrigger = std::max(rightTrigger, g_controls[i].rightTrigger);
        }
    }

    // Swipes produce a short look impulse that fades out, so the camera stops when the finger does.
    if (g_lookTime >= 0.0)
    {
        double age = time - g_lookTime;

        if (age < LOOK_HOLD_SECONDS)
        {
            float decay = float(1.0 - age / LOOK_HOLD_SECONDS);
            rightX = ToAxis(g_lookVector.x * decay);
            rightY = ToAxis(g_lookVector.y * decay);
        }
    }

    pad.wButtons |= buttons;
    pad.bLeftTrigger = std::max(pad.bLeftTrigger, leftTrigger);
    pad.bRightTrigger = std::max(pad.bRightTrigger, rightTrigger);
    MergeAxis(pad.sThumbLX, leftX);
    MergeAxis(pad.sThumbLY, leftY);
    MergeAxis(pad.sThumbRX, rightX);
    MergeAxis(pad.sThumbRY, rightY);
}

void TouchControls::Draw(float swapChainWidth, float swapChainHeight, float viewportOffsetX, float viewportOffsetY)
{
    if (!IsActive())
        return;

    std::lock_guard lock(g_mutex);

    auto drawList = ImGui::GetBackgroundDrawList();
    float scale = swapChainWidth / g_windowSize.x;
    float opacity = std::clamp(Config::TouchControlsOpacity.Value, 0.0f, 1.0f);

    auto toScreen = [&](ImVec2 point)
    {
        return ImVec2(point.x * scale - viewportOffsetX, point.y * scale - viewportOffsetY);
    };

    auto white = [&](float alpha)
    {
        return IM_COL32(255, 255, 255, int(std::clamp(alpha * opacity, 0.0f, 1.0f) * 255.0f));
    };

    auto isControlHeld = [&](size_t index)
    {
        for (auto& capture : g_captures)
        {
            if (capture.control == int(index))
                return true;
        }

        return false;
    };

    for (size_t i = 0; i < CONTROL_COUNT; i++)
    {
        const TouchControl& control = g_controls[i];
        TouchRect frame = GetControlFrame(control, g_windowSize);
        ImVec2 centre = toScreen(GetCentre(frame));
        float borderWidth = std::max(1.5f * scale, 1.0f);
        bool held = isControlHeld(i);

        if (control.kind == TouchControlKind::Stick)
        {
            float radius = GetRadius(frame) * scale;
            float ringRadius = std::min(frame.width, frame.height) * STICK_DPAD_RING_RADIUS * scale;

            drawList->AddCircleFilled(centre, radius, white(0.10f), 48);
            drawList->AddCircle(centre, radius, white(0.38f), 48, borderWidth);
            drawList->AddCircle(centre, ringRadius, white(0.20f), 48, borderWidth);

            // D-Pad arrows on the outer ring.
            float arrowDistance = (radius + ringRadius) * 0.5f;
            float arrowSize = (radius - ringRadius) * 0.3f;
            const ImVec2 directions[] = { { 0.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f }, { 1.0f, 0.0f } };
            const StickZone zones[] = { StickZone::DpadUp, StickZone::DpadDown, StickZone::DpadLeft, StickZone::DpadRight };

            for (size_t d = 0; d < 4; d++)
            {
                bool pressed = std::any_of(g_captures.begin(), g_captures.end(), [&](const TouchCapture& c) { return c.control == int(i) && c.stickZone == zones[d]; });
                ImVec2 dir = directions[d];
                ImVec2 tip = { centre.x + dir.x * (arrowDistance + arrowSize), centre.y + dir.y * (arrowDistance + arrowSize) };
                ImVec2 base = { centre.x + dir.x * (arrowDistance - arrowSize), centre.y + dir.y * (arrowDistance - arrowSize) };
                ImVec2 side = { -dir.y * arrowSize, dir.x * arrowSize };

                drawList->AddTriangleFilled(tip, { base.x + side.x, base.y + side.y }, { base.x - side.x, base.y - side.y }, white(pressed ? 0.85f : 0.45f));
            }

            // Knob follows the finger while the stick is held.
            ImVec2 knob = centre;
            for (auto& capture : g_captures)
            {
                if (capture.control == int(i) && capture.stickZone == StickZone::Stick)
                {
                    float outerRadius = std::min(frame.width, frame.height) * STICK_ACTIVATION_RADIUS;
                    ImVec2 delta = { capture.current.x - capture.anchor.x, capture.current.y - capture.anchor.y };
                    float distance = std::hypot(delta.x, delta.y);

                    if (distance > outerRadius && distance > 0.0f)
                    {
                        delta.x *= outerRadius / distance;
                        delta.y *= outerRadius / distance;
                    }

                    knob = { centre.x + delta.x * scale, centre.y + delta.y * scale };
                }
            }

            float knobRadius = std::min(frame.width, frame.height) * 0.14f * scale;
            drawList->AddCircleFilled(knob, knobRadius, white(0.18f), 32);
            drawList->AddCircle(knob, knobRadius, white(0.72f), 32, borderWidth);
        }
        else
        {
            ImVec2 min = toScreen({ frame.x, frame.y });
            ImVec2 max = toScreen({ frame.x + frame.width, frame.y + frame.height });
            float rounding = GetRadius(frame) * scale;

            drawList->AddRectFilled(min, max, white(held ? 0.35f : 0.08f), rounding);
            drawList->AddRect(min, max, white(held ? 0.80f : 0.32f), rounding, 0, borderWidth);

            if (g_font != nullptr)
            {
                float fontSize = std::min(frame.width, frame.height) * (strlen(control.label) > 2 ? 0.30f : 0.42f) * scale;
                ImVec2 textSize = g_font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, control.label);
                drawList->AddText(g_font, fontSize, { centre.x - textSize.x * 0.5f, centre.y - textSize.y * 0.5f }, white(held ? 1.0f : 0.85f), control.label);
            }
        }
    }
}
