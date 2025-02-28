/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkString.h"
#include "include/core/SkTiledImageUtils.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkRandom.h"

int make_bm(SkBitmap* bm, int height) {
    constexpr int kRadius = 22;
    constexpr int kMargin = 8;
    constexpr SkScalar kStartAngle = 0;
    constexpr SkScalar kDAngle = 25;
    constexpr SkScalar kSweep = 320;
    constexpr SkScalar kThickness = 8;

    int count = (height / (2 * kRadius + kMargin));
    height = count * (2 * kRadius + kMargin);

    bm->allocN32Pixels(2 * (kRadius + kMargin), height);
    SkRandom random;

    SkCanvas wholeCanvas(*bm);
    wholeCanvas.clear(0x00000000);

    SkScalar angle = kStartAngle;
    for (int i = 0; i < count; ++i) {
        SkPaint paint;
        // The sw rasterizer disables AA for large canvii. So we make a small canvas for each draw.
        SkBitmap smallBM;
        SkIRect subRect = SkIRect::MakeXYWH(0, i * (kMargin + 2 * kRadius),
                                            2 * kRadius + kMargin, 2 * kRadius + kMargin);
        bm->extractSubset(&smallBM, subRect);
        SkCanvas canvas(smallBM);
        canvas.translate(kMargin + kRadius, kMargin + kRadius);

        paint.setAntiAlias(true);
        paint.setColor(random.nextU() | 0xFF000000);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(kThickness);
        paint.setStrokeCap(SkPaint::kRound_Cap);
        SkScalar radius = kRadius - kThickness / 2;
        SkRect bounds = SkRect::MakeLTRB(-radius, -radius, radius, radius);

        canvas.drawArc(bounds, angle, kSweep, false, paint);
        angle += kDAngle;
    }
    bm->setImmutable();
    return count;
}

class TallStretchedBitmapsGM : public skiagm::GM {
public:
    TallStretchedBitmapsGM() {}

protected:
    SkString getName() const override { return SkString("tall_stretched_bitmaps"); }

    SkISize getISize() override { return SkISize::Make(730, 690); }

    void onOnceBeforeDraw() override {
        for (size_t i = 0; i < std::size(fTallBmps); ++i) {
            int h = SkToInt((4 + i) * 1024);

            fTallBmps[i].fItemCnt = make_bm(&fTallBmps[i].fBmp, h);
        }
    }

    void onDraw(SkCanvas* canvas) override {
        canvas->scale(1.3f, 1.3f);
        for (size_t i = 0; i < std::size(fTallBmps); ++i) {
            SkASSERT(fTallBmps[i].fItemCnt > 10);
            SkBitmap bmp = fTallBmps[i].fBmp;
            // Draw the last 10 elements of the bitmap.
            int startItem = fTallBmps[i].fItemCnt - 10;
            int itemHeight = bmp.height() / fTallBmps[i].fItemCnt;
            SkIRect subRect = SkIRect::MakeLTRB(0, startItem * itemHeight,
                                               bmp.width(), bmp.height());
            SkRect dstRect = SkRect::MakeWH(SkIntToScalar(bmp.width()), 10.f * itemHeight);
            SkTiledImageUtils::DrawImageRect(canvas, bmp.asImage(),
                                             SkRect::Make(subRect), dstRect,
                                             SkSamplingOptions(SkFilterMode::kLinear), nullptr,
                                             SkCanvas::kStrict_SrcRectConstraint);
            canvas->translate(SkIntToScalar(bmp.width() + 10), 0);
        }
    }

private:
    struct {
        SkBitmap fBmp;
        int      fItemCnt;
    } fTallBmps[8];
    using INHERITED = skiagm::GM;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new TallStretchedBitmapsGM;)
