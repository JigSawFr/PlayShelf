// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/qr_view.hpp"

#include <cmath>
#include <qrcodegen.hpp>

QrView::QrView(const std::string& text, float side)
{
    this->setWidth(side);
    this->setHeight(side);
    this->setFocusable(false);
    try {
        // Medium correction: a URL stays small (version 3 or 4) and still reads
        // off a screen with some glare.
        const auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        this->size = qr.getSize();
        this->modules.resize((size_t)size * size);
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) this->modules[(size_t)y * size + x] = qr.getModule(x, y);
    } catch (const std::exception& e) {
        brls::Logger::error("QR code of \"{}\": {}", text, e.what());
        this->size = 0;
    }
}

QrView::~QrView()
{
    if (this->image) nvgDeleteImage(brls::Application::getNVGContext(), this->image);
}

void QrView::draw(NVGcontext* vg, float x, float y, float width, float height,
                  brls::Style, brls::FrameContext*)
{
    if (this->size == 0) return;
    const int   quiet  = 4;
    const int   pixels = this->size + 2 * quiet;   // the texture: one pixel per module, quiet zone included
    const float side   = std::fmin(width, height);
    const float module = std::floor(side / pixels);
    if (module < 1) return;

    if (!this->image) {
        std::vector<unsigned char> rgba((size_t)pixels * pixels * 4, 0xFF);   // white, opaque
        for (int r = 0; r < this->size; r++)
            for (int c = 0; c < this->size; c++)
                if (this->modules[(size_t)r * this->size + c]) {
                    unsigned char* p = &rgba[(((size_t)(quiet + r) * pixels) + quiet + c) * 4];
                    p[0] = p[1] = p[2] = 0x00;
                }
        this->image = nvgCreateImageRGBA(vg, pixels, pixels, NVG_IMAGE_NEAREST, rgba.data());
        if (!this->image) return;
    }

    const float code = module * pixels;
    const float left = std::round(x + (width - code) / 2), top = std::round(y + (height - code) / 2);
    nvgBeginPath(vg);
    // Rounded within the quiet zone, as the boards' cards: no module is cut.
    nvgRoundedRect(vg, left, top, code, code, module * quiet / 2);
    nvgFillPaint(vg, nvgImagePattern(vg, left, top, code, code, 0, this->image, 1.0f));
    nvgFill(vg);
}
