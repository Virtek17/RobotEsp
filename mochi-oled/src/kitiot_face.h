// Adapted from https://mochi.kitiot.com/; all 20 Mochi presets.
// Regenerate with node mochi-source/import-kitiot.cjs
#pragma once
namespace KitiotFace {
enum Emotion {
  EMO_ALIVE, // ALIVE 
  EMO_HAPPY, // HAPPY 
  EMO_LOVE, // LOVE 
  EMO_ANGRY, // ANGRY 
  EMO_FURIOUS, // FURIOUS 
  EMO_SAD, // SAD 
  EMO_DIZZY, // DIZZY 
  EMO_SLEEPY, // SLEEPY 
  EMO_DEEP_SLEEP, // DEEP_SLEEP 
  EMO_CONFUSED, // CONFUSED 
  EMO_SURPRISED, // SURPRISED 
  EMO_WINK, // WINK 
  EMO_CAT, // CAT 
  EMO_SMILE, // SMILE 
  EMO_LAUGH, // LAUGH 
  EMO_SMUG, // SMUG 
  EMO_STAR_EYES, // STAR_EYES 
  EMO_COOL, // COOL 
  EMO_CHEEKY, // CHEEKY 
  EMO_DERP, // DERP 
};

Emotion emotionsList[] = {
  EMO_ALIVE,
  EMO_HAPPY,
  EMO_LOVE,
  EMO_ANGRY,
  EMO_FURIOUS,
  EMO_SAD,
  EMO_DIZZY,
  EMO_SLEEPY,
  EMO_DEEP_SLEEP,
  EMO_CONFUSED,
  EMO_SURPRISED,
  EMO_WINK,
  EMO_CAT,
  EMO_SMILE,
  EMO_LAUGH,
  EMO_SMUG,
  EMO_STAR_EYES,
  EMO_COOL,
  EMO_CHEEKY,
  EMO_DERP,
};
const int TOTAL_EMOTIONS = 20;
int currentEmoIndex = 0;
Emotion activeEmotion = EMO_ALIVE;

// Tùy chọn: Tự động đổi biểu cảm theo nhịp sinh học ngẫu nhiên
#define AUTO_CYCLE_EMOTIONS false
unsigned long nextEmoSwitchTime = 4000;

// 3. BỘ ĐẾM HOẠT HỌA TỰ NHIÊN HỮU CƠ (ORGANIC IDLE ANIMATIONS)
// Tọa độ liếc nhìn có nội suy lướt mượt (Smooth Saccade Interpolation)
float curEyeOx = 0.0;
float curEyeOy = 0.0;
float targetEyeOx = 0.0;
float targetEyeOy = 0.0;
int eyeOx = 0;
int eyeOy = 0;
unsigned long nextLookTime = 0;
unsigned long lookResetTime = 0;

// Hệ thống chớp mắt mượt mà đa bước (Smooth Organic Blink Curve)
bool isBlinking = false;
unsigned long blinkStartTime = 0;
unsigned long blinkDuration = 140;
unsigned long nextBlinkTime = 0;
float blinkFactor = 1.0; // 1.0 = mở to, 0.06 = nhắm tịt

bool isYawning = false;
unsigned long nextYawnTime = 25000;
unsigned long yawnEndTime = 0;
int mouthExtraH = 0;

void updateIdleAnimations() {
  unsigned long now = millis();

  // 1. Tự động chuyển biểu cảm định kỳ với kỹ thuật Blink-to-Transition
  if (AUTO_CYCLE_EMOTIONS && now >= nextEmoSwitchTime) {
    currentEmoIndex = (currentEmoIndex + 1) % TOTAL_EMOTIONS;
    activeEmotion = emotionsList[currentEmoIndex];
    nextEmoSwitchTime = now + random(3500, 6000); // Ngẫu nhiên 3.5 - 6.0 giây

    // Chớp mắt một nhịp nhanh để chuyển biểu cảm mượt mà (Blink-to-Transition)
    isBlinking = true;
    blinkStartTime = now;
    blinkDuration = 120;
  }

  // 2. Chớp mắt tự nhiên (kèm khả năng chớp đôi ngẫu nhiên)
  if (!isBlinking && now >= nextBlinkTime) {
    isBlinking = true;
    blinkStartTime = now;
    blinkDuration = random(120, 160);
    // Nhịp chớp tiếp theo: ngẫu nhiên từ 2.0s đến 5.5s
    nextBlinkTime = now + random(2000, 5500);
    // 20% khả năng chớp đôi (double blink) sau 260ms
    if (random(100) < 20) {
      nextBlinkTime = now + random(240, 340);
    }
  }

  // Tính toán hệ số chớp mắt mượt mà theo đường cong Sin (Smooth Curve)
  if (isBlinking) {
    unsigned long elapsed = now - blinkStartTime;
    if (elapsed < blinkDuration) {
      float progress = (float)elapsed / (float)blinkDuration; // 0.0 -> 1.0
      // Đạt cực đại nhắm mắt tại giữa nhịp (progress = 0.5)
      blinkFactor = max(0.06f, 1.0f - sin(progress * 3.14159f) * 0.94f);
    } else {
      isBlinking = false;
      blinkFactor = 1.0f;
    }
  } else {
    blinkFactor = 1.0f;
  }

  // 3. Liếc nhìn tự nhiên (Saccade) kèm nội suy lướt mượt (Smooth Easing)
  if (now >= nextLookTime) {
    targetEyeOx = (float)random(-4, 5);
    targetEyeOy = (float)random(-3, 4);
    nextLookTime = now + random(2500, 5000);
    lookResetTime = now + random(1400, 2200); // Tự động trở về tâm sau 1.4 - 2.2 giây
  }
  // Tự động kéo mắt về tâm
  if (lookResetTime > 0 && now >= lookResetTime) {
    targetEyeOx = 0.0;
    targetEyeOy = 0.0;
    lookResetTime = 0;
  }
  // Nội suy lướt êm ái tới toạ độ đích (Damping)
  curEyeOx += (targetEyeOx - curEyeOx) * 0.22f;
  curEyeOy += (targetEyeOy - curEyeOy) * 0.22f;
  eyeOx = (int)round(curEyeOx);
  eyeOy = (int)round(curEyeOy);

  // 4. Ngáp ngẫu nhiên (mỗi 30 - 65 giây)
  if (!isYawning && now >= nextYawnTime) {
    isYawning = true;
    mouthExtraH = 11;
    yawnEndTime = now + 850;
    nextYawnTime = now + random(30000, 65000);
  }
  if (isYawning && now >= yawnEndTime) {
    isYawning = false;
    mouthExtraH = 0;
  }
}

// 4. CÁC HÀM VẼ ĐẶC BIỆT (AN TOÀN TUYỆT ĐỐI CHO U8G2)

// Hàm vẽ mắt bo góc an toàn tuyệt đối (có xén viền Boundary Clipping, chống tràn số khi trượt sang trái)
void drawEye(int x, int y, int w, int h, int r) {
  if (y + h <= 0 || y >= 64) return;
  if (x + w <= 0 || x >= 128) return; // Ra hẳn ngoài màn hình -> bỏ qua

  // Xén mép trái khi trượt sang trái (x < 0)
  if (x < 0) {
    w += x;
    x = 0;
    if (w <= 0) return;
    display.drawBox(x, y, w, h);
    return;
  }
  // Xén mép phải khi trượt sang phải (x + w > 128)
  if (x + w > 128) {
    w = 128 - x;
    if (w <= 0) return;
    display.drawBox(x, y, w, h);
    return;
  }

  int safeR = min(r, min(w / 2, h / 2));
  if (h <= 5 || w <= 5 || safeR <= 1) {
    display.drawBox(x, y, w, h);
  } else {
    display.drawRBox(x, y, w, h, safeR);
  }
}

// Biểu cảm Chóng Mặt 🌀: Mắt xoắn ốc quay tròn liên tục
float spiralAngle = 0.0;
void drawSpiral(int centerX, int centerY, int direction, float rotationOffset) {
  float angle = 0; float radius = 0;
  while (radius < 13) { 
    float effectiveAngle = (angle + rotationOffset) * direction;
    int x = centerX + (int)(cos(effectiveAngle) * radius);
    int y = centerY + (int)(sin(effectiveAngle) * radius);
    display.drawPixel(x, y);
    display.drawPixel(x + 1, y);
    angle += 0.4; radius += 0.28; 
  }
}

// Biểu cảm Yêu Thích ❤️: Mắt trái tim đập phập phồng
void drawHeart(int x, int y, float scale) {
  int r = 6 * scale; 
  int offX = 6 * scale; 
  int offY = 4 * scale; 
  int triH = 12 * scale;
  display.drawDisc(x - offX, y - offY, r);
  display.drawDisc(x + offX, y - offY, r);
  display.drawTriangle(x - (r * 2), y - offY, x + (r * 2), y - offY, x, y + triH);
}

// Biểu cảm Buồn Bã 😭: Nước mắt rơi tí tách
float tearProgress = 0;
void drawTears(int leftX, int rightX, int startY) {
  for (int i = 0; i < 4; i++) {
    int dropY = startY + ((int)(tearProgress + i * 7) % 22);
    display.drawDisc(leftX, dropY, 1);
    display.drawDisc(rightX, dropY, 1);
  }
  tearProgress += 0.5;
}

// Biểu tượng Gân Nổi Giận Anime 💢 (4 góc cong chỉa vào tâm)
void drawAngryVein(int cx, int cy) {
  int pulse = (millis() / 150) % 2; 
  int r = pulse == 0 ? 6 : 8;
  int elbow = pulse == 0 ? 3 : 4;
  int signs[4][2] = {{1, -1}, {-1, -1}, {-1, 1}, {1, 1}};
  for (int i = 0; i < 4; i++) {
    int sx = signs[i][0]; int sy = signs[i][1];
    display.drawLine(cx + sx * 2, cy + sy * r, cx + sx * elbow, cy + sy * elbow);
    display.drawLine(cx + sx * 3, cy + sy * r, cx + sx * (elbow + 1), cy + sy * (elbow + 1));
    display.drawLine(cx + sx * elbow, cy + sy * elbow, cx + sx * r, cy + sy * 2);
    display.drawLine(cx + sx * (elbow + 1), cy + sy * (elbow + 1), cx + sx * r, cy + sy * 3);
  }
}

// ===== BIỂU CẢM VUI / BỰA: nét vẽ cơ bản (1 màu nét + màu nền) =====
#define FF_W 128
#define FF_H 64
void ffOn() { display.setDrawColor(1); }
void ffOff() { display.setDrawColor(0); }
void ffPx(int x, int y) { if (x >= 0 && y >= 0 && x < FF_W && y < FF_H) display.drawPixel(x, y); }
void ffBox(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > FF_W) w = FF_W - x;
  if (y + h > FF_H) h = FF_H - y;
  if (w > 0 && h > 0) display.drawBox(x, y, w, h);
}
void ffRBox(int x, int y, int w, int h, int r) {
  r = min(r, min(w, h) / 2 - 1);
  if (r < 1 || x < 0 || y < 0 || x + w > FF_W || y + h > FF_H) { ffBox(x, y, w, h); return; }
  display.drawRBox(x, y, w, h, r);
}
void ffTri(int x0, int y0, int x1, int y1, int x2, int y2) { display.drawTriangle(x0, y0, x1, y1, x2, y2); }
void ffLine(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    ffPx(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
// Nét dày 2px
void ffThick(int x0, int y0, int x1, int y1) {
  ffLine(x0, y0, x1, y1); ffLine(x0 + 1, y0, x1 + 1, y1);
  ffLine(x0, y0 + 1, x1, y1 + 1); ffLine(x0 + 1, y0 + 1, x1 + 1, y1 + 1);
}
void ffDisc(int x, int y, int r) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = (int)sqrt((float)(r * r - dy * dy));
    ffBox(x - dx, y + dy, 2 * dx + 1, 1);
  }
}
// Nét bo tròn dày (2r+1) — cùng độ dày miệng Mochi
void ffCapsule(int x0, int y0, int x1, int y1, int r) {
  int n = max(max(abs(x1 - x0), abs(y1 - y0)), 1);
  for (int i = 0; i <= n; i++) ffDisc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, r);
}
// Nửa dưới hình elip đặc (miệng cười)
void ffHalfEllipse(int cx, int top, int rx, int ry) {
  for (int dy = 0; dy <= ry; dy++) {
    int dx = (int)(rx * sqrt(1.0f - (float)(dy * dy) / (float)(ry * ry)));
    ffBox(cx - dx, top + dy, 2 * dx + 1, 1);
  }
}
float ffEase(float p) { return 0.5f - 0.5f * cos(p * 3.14159f); }

// Mắt cười cong ∩: khối mắt Mochi bị khoét phần dưới
void ffHappyEye(int cx, int top, int ew, int hh) {
  int th = max(4, (int)(hh * 0.45f));
  int r = min(ew, hh) / 2;
  int x = cx - ew / 2;
  ffOn(); ffRBox(x, top, ew, hh, r);
  ffOff(); ffRBox(x - 1, top + th, ew + 2, hh, r);
  ffOn();
}
// Nhếch mép: thanh đặc dày bằng miệng Mochi, cong lên 1 bên
void ffSmirk(int mx, int my, int mw, int mh) {
  int a = mw / 2, r = max(1, mh / 2);
  ffOn();
  ffCapsule(mx - a, my, mx + a - 2, my - 2, r);
  ffCapsule(mx + a - 2, my - 2, mx + a + 1, my - 5, r);
}
// Miệng cười mỉm: nửa elip đặc bị khoét bên trong
void ffSmileMouth(int cx, int top, int rx, int ry) {
  int th = max(3, ry / 2);
  ffOn(); ffHalfEllipse(cx, top, rx, ry);
  ffOff(); ffHalfEllipse(cx, top - 1, rx - th, ry - th);
  ffOn();
}

// Đang cười 😊: mắt cười cong + miệng cười mỉm, nhịp thở nhẹ
void ffSmile(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  int dy = (int)(sin(t * 0.003f) * 1.5f);
  int hh = max(6, (int)(eh * 0.55f * bf));
  ffHappyEye(lx, ly - (int)(eh * 0.3f) + dy, ew, hh);
  ffHappyEye(rx, ry - (int)(eh * 0.3f) + dy, ew, hh);
  int rxm = (int)(mw * 0.8f), rym = max(4, (int)(mw * 0.35f));
  ffSmileMouth(mx, min(my - mh / 2, FF_H - 2 - rym) + dy, rxm, rym);
}

// Cười lăn 😆: mắt híp cong + miệng há cười "ha ha" + nảy người + nước mắt vui
void ffLaugh(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  int dy = -(int)(fabs(sin(t * 0.0126f)) * (FF_H / 16.0f));
  int sx = (int)(sin(t * 0.025f) * 2);
  int hh = max(6, (int)(eh * 0.42f));
  int ey = ly - (int)(eh * 0.25f) + dy;
  ffHappyEye(lx + sx, ey, ew, hh);
  ffHappyEye(rx + sx, ry - (int)(eh * 0.25f) + dy, ew, hh);
  int ty = ey + hh + (int)(((t % 900) / 900.0f) * eh * 0.35f);
  ffDisc(lx - ew / 2 - 2 + sx, ty, 1);
  ffDisc(rx + ew / 2 + 1 + sx, ty, 1);
  int ryMax = max(5, (int)(mw * 0.7f));
  int rym = max(3, (int)(ryMax * (0.6f + 0.4f * fabs(sin(t * 0.0126f)))));
  ffHalfEllipse(mx + sx, min(my - mh / 2, FF_H - 2 - ryMax) + dy, (int)(mw * 0.85f), rym);
}

// Nham hiểm 😏: mí sụp gian xảo, liếc ngang, nhướng mày 2 cái, nhếch mép
void ffSmugEye(int cx, int cy, int ew, int h, int lid, int slant, bool innerRight) {
  int x = cx - ew / 2;
  int y = cy - h / 2;
  ffOn(); ffRBox(x, y, ew, h, (int)(min(ew, h) * 0.4f));
  ffOff(); ffBox(x - 1, y - 1, ew + 2, lid + 1);
  if (innerRight) ffTri(x - 1, y + lid, x + ew + 1, y + lid, x + ew + 1, y + lid + slant);
  else ffTri(x - 1, y + lid, x + ew + 1, y + lid, x - 1, y + lid + slant);
}
void ffSmug(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long c = millis() % 4200;
  int G = ew / 4;
  int gx = 0;
  if (c >= 800 && c < 1200) gx = (int)(G * ffEase((c - 800) / 400.0f));
  else if (c >= 1200 && c < 3400) gx = G;
  else if (c >= 3400 && c < 3800) gx = (int)(G * (1.0f - ffEase((c - 3400) / 400.0f)));
  int h = max(4, (int)(eh * bf));
  int lidBase = (int)(h * 0.48f);
  bool wiggle = (c >= 1600 && c < 1800) || (c >= 2000 && c < 2200);
  int slant = max(2, (int)(eh * 0.16f));
  ffSmugEye(lx + gx, ly, ew, h, wiggle ? (int)(h * 0.22f) : lidBase, slant, true);
  ffSmugEye(rx + gx, ry, ew, h, lidBase, slant, false);
  ffSmirk(mx + gx / 2, my, mw, mh);
}

// Mắt sao 🤩: ngôi sao 4 cánh đập nhịp + tia lấp lánh + miệng chữ O ngạc nhiên
void ffStar(int cx, int cy, int Rh, int Rv, int k) {
  ffTri(cx, cy - Rv, cx - k, cy, cx + k, cy);
  ffTri(cx, cy + Rv, cx - k, cy, cx + k, cy);
  ffTri(cx - Rh, cy, cx, cy - k, cx, cy + k);
  ffTri(cx + Rh, cy, cx, cy - k, cx, cy + k);
}
void ffStarEyes(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  float pulse = 0.85f + 0.15f * sin(t * 0.008f);
  int Rh = max(5, (int)(ew * 0.62f * pulse));
  int Rv = max(5, (int)(eh * 0.45f * pulse * bf));
  int k = max(2, Rh / 3);
  ffOn();
  ffStar(lx, ly, Rh, Rv, k);
  ffStar(rx, ry, Rh, Rv, k);
  int tw[3][2] = {{lx - ew, ly - (int)(eh * 0.35f)}, {rx + ew, ry - (int)(eh * 0.15f)}, {(lx + rx) / 2, ly - (int)(eh * 0.45f)}};
  unsigned long phase = t / 300;
  for (int i = 0; i < 3; i++) {
    if ((phase + i) % 3 != 0) {
      ffLine(tw[i][0] - 2, tw[i][1], tw[i][0] + 2, tw[i][1]);
      ffLine(tw[i][0], tw[i][1] - 2, tw[i][0], tw[i][1] + 2);
    }
  }
  float gasp = 0.85f + 0.15f * sin(t * 0.006f);
  int ow = max(6, (int)(mw * 0.6f * gasp));
  int oh = max(6, (int)(mw * 0.8f * gasp));
  int oy = min(my - oh / 2, FF_H - 1 - oh);
  ffRBox(mx - ow / 2, oy, ow, oh, min(ow, oh) / 2);
}

// Ngầu 😎: kính đen lóe sáng + gật gù + nhếch mép
void ffCool(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  int dy = (int)(sin(t * 0.004f) * 1.5f);
  int gw = (int)(ew * 1.35f), gh = max(6, (int)(eh * 0.42f));
  int gy = ly - gh / 2 - 2 + dy;
  int xl = lx - gw / 2, xr = rx - gw / 2;
  ffOn();
  ffRBox(xl, gy, gw, gh, 3);
  ffRBox(xr, gy, gw, gh, 3);
  ffThick(xl + gw - 1, gy + 1, xr, gy + 1);
  ffThick(xl, gy + 1, xl - 5, gy - 1);
  ffThick(xr + gw - 1, gy + 1, xr + gw + 4, gy - 1);
  unsigned long p = t % 2600;
  if (p < 500) {
    int x = xl + (int)((p / 500.0f) * (gw + gh));
    ffOff();
    ffThick(x, gy + 1, x - gh + 2, gy + gh - 2);
    ffThick(x + (xr - xl), gy + 1, x + (xr - xl) - gh + 2, gy + gh - 2);
  }
  ffSmirk(mx, my + dy, mw, mh);
}

// Lè lưỡi 😛: nheo 1 mắt + lè lưỡi ngoáy trêu ngươi
void ffCheeky(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  int tilt = (int)(sin(t * 0.003f) * 3);
  int h = max(3, (int)(eh * bf));
  ffOn();
  ffRBox(lx - ew / 2 + tilt, ly - h / 2, ew, h, (int)(min(ew, h) * 0.4f));
  int sw = (int)(ew * 1.1f);
  ffRBox(rx - sw / 2 + tilt, ry + (int)(eh * 0.08f), sw, 4, 1);
  int bw = (int)(mw * 0.7f);
  int bh = max(3, mh);
  ffRBox(mx - bw + tilt, my - bh + 1, 2 * bw, bh, 1);
  int tw = max(6, (int)(mw * 0.8f));
  int th = max(4, min(10, FF_H - my - 1));
  int tx = mx - tw / 2 + tilt + (int)(sin(t * 0.02f) * 3);
  ffRBox(tx, my - 1, tw, th, 3);
  ffOff();
  ffLine(tx + tw / 2, my + 1, tx + tw / 2, my + th - 4);
}

// Ngáo 🤪: mắt to mắt nhỏ xoay vòng ngược nhau + miệng méo + răng thỏ
void ffDerp(int lx, int ly, int rx, int ry, int ew, int eh, int mx, int my, int mw, int mh, float bf) {
  unsigned long t = millis();
  int bw = (int)(ew * 1.1f), bh = max(3, (int)(eh * 0.95f * bf));
  int sw = (int)(ew * 0.6f), sh = max(3, (int)(eh * 0.5f * bf));
  float a1 = t * 0.006f, a2 = -(t * 0.009f) + 1;
  int l0 = lx + (int)(cos(a1) * 3), l1 = ly + (int)(sin(a1) * 3);
  int r0 = rx + (int)(cos(a2) * 3), r1 = ry + 4 + (int)(sin(a2) * 3);
  ffOn();
  ffRBox(l0 - bw / 2, l1 - bh / 2, bw, bh, (int)(min(bw, bh) * 0.45f));
  ffRBox(r0 - sw / 2, r1 - sh / 2, sw, sh, (int)(min(sw, sh) * 0.45f));
  int rr = max(1, mh / 2);
  int px = 0, py = 0;
  for (int s = 0; s <= 6; s++) {
    int x = mx - mw + (2 * mw * s) / 6;
    int y = my - 2 + (int)(sin(s * 1.3f + t * 0.01f) * 1.5f);
    if (s > 0) ffCapsule(px, py, x, y, rr);
    px = x; py = y;
  }
  ffBox(mx - 4, my, 3, 5);
  ffBox(mx + 1, my, 3, 5);
}



// Biểu cảm Ngủ Say 💤: Vẽ chữ Z bay lên
void drawZ(int x, int y, int s) {
  display.drawLine(x, y, x + s, y);
  display.drawLine(x + s, y, x, y + s);
  display.drawLine(x, y + s, x + s, y + s);
}

// 5. HÀM VẼ KHUÔN MẶT BẰNG U8G2
void drawFace(Emotion emo, bool flush = true) {
  display.clearBuffer();

  switch (emo) {
    case EMO_ALIVE: // ALIVE 
      {
        int baseH = isYawning ? 18 : 40;
        int curH = max(2, (int)(baseH * blinkFactor));
        int curY = 6 + (40 - curH) / 2 + (isYawning ? 8 : 0);
        drawEye(28 + eyeOx, curY + eyeOy, 24, curH, 10);
        drawEye(76 + eyeOx, curY + eyeOy, 24, curH, 10);
      }
      // Miệng (kèm hiệu ứng há to khi ngáp)
      display.drawRBox(56 + eyeOx / 2, 53 - mouthExtraH / 2, 16, 4 + mouthExtraH, 1);
      break;

    case EMO_HAPPY: // Vui vẻ 😄 (Mắt mở to nhỏ ngạc nhiên vui vẻ + Huýt sáo nhấp nhô)
      {
        // 1. Mắt mở to nhỏ kiểu ngạc nhiên vui vẻ (Joyful surprised eye pulse)
        float eyeJoyTime = millis() * 0.005f;
        float eyePulseH = 0.95f + sin(eyeJoyTime) * 0.18f; // Co giãn mở to - nhỏ
        float eyePulseW = 0.96f + cos(eyeJoyTime) * 0.08f;
        int curEyeH = max(4, (int)(38 * eyePulseH * blinkFactor));
        int curEyeW = max(4, (int)(21 * eyePulseW));
        int curY = 2 + (38 - curEyeH) / 2;
        int curLx = 30 + (21 - curEyeW) / 2;
        int curRx = 78 + (21 - curEyeW) / 2;
        drawEye(curLx + eyeOx, curY + eyeOy, curEyeW, curEyeH, 7);
        drawEye(curRx + eyeOx, curY + eyeOy, curEyeW, curEyeH, 7);

        // 2. Hoạt ảnh huýt sáo: Miệng chu ngắn lại và nhấp nhô cao -> thấp -> cao
        float whistleTime = millis() * 0.0055f;
        int whistleBobY = (int)(sin(whistleTime) * 4.5f); // Nhấp nhô: cao lên rồi thấp lại
        int whistleW = max(6, (int)(29 * 0.40f + sin(whistleTime * 2.0f) * 1.5f)); // Miệng ngắn lại
        int whistleH = max(6, (int)(9 * 1.25f + cos(whistleTime * 2.0f) * 1.0f));
        int whistleX = 50 + eyeOx / 2 + (29 - whistleW) / 2;
        int whistleY = 51 - mouthExtraH / 2 - whistleBobY;
        display.drawRBox(whistleX, whistleY, whistleW, whistleH, min(whistleW, whistleH) / 2);

        // Nốt nhạc nhỏ bay lên bên cạnh miệng huýt sáo ♪
        int notePhase = (millis() / 28) % 45;
        int noteX = whistleX + whistleW + 4 + notePhase / 3;
        int noteY = whistleY - 4 - notePhase / 2;
        if (noteY > 5 && noteX < 122) {
          display.drawDisc(noteX, noteY, 1);
          display.drawLine(noteX + 1, noteY, noteX + 1, noteY - 5);
          display.drawLine(noteX + 1, noteY - 5, noteX + 3, noteY - 4);
        }
      }
      break;

    case EMO_LOVE: // Yêu thích ❤️ (Mắt trái tim đập phập phồng)
      {
        float heartScale = 0.85 + 0.2 * abs(sin(millis() * 0.005));
        drawHeart(40 + eyeOx, 26 + eyeOy, heartScale);
        drawHeart(88 + eyeOx, 26 + eyeOy, heartScale);
        // Miệng cười nhỏ
        display.drawRBox(52 + eyeOx / 2, 51, 24, 8, 2);
      }
      break;

    case EMO_ANGRY: // ANGRY 
      drawAngryVein(95, 15); // Gân giận 💢 bên phải (chuẩn Anime)
      {
        int baseH = isYawning ? 10 : 22;
        int curH = max(2, (int)(baseH * blinkFactor));
        int curY = 19 + (22 - curH) / 2 + (isYawning ? 4 : 0);
        drawEye(37 + eyeOx, curY + eyeOy, 22, curH, 9);
        drawEye(69 + eyeOx, curY + eyeOy, 22, curH, 9);
      }
      // Miệng (kèm hiệu ứng há to khi ngáp)
      display.drawRBox(53 + eyeOx / 2, 53 - mouthExtraH / 2, 22, 4 + mouthExtraH, 1);
      break;

    case EMO_FURIOUS: // Tức tím người 🤬 (Mắt trái giật giật + Miệng mấp máy muốn chửi + Gân 💢)
      drawAngryVein(95, 14); // Gân giận 💢 bên phải (chuẩn Anime)
      {
        // 1. Mắt trái co giật mí mắt liên hồi (Left eye twitching)
        bool twitch = (millis() / 55) % 3 != 0;
        int lEyeH = twitch ? max(2, (int)(20 * 0.62 * blinkFactor)) : max(2, (int)(20 * blinkFactor));
        int lEyeY = 20 + (20 - lEyeH) / 2 + (twitch ? 1 : 0);
        drawEye(39 + eyeOx, lEyeY + eyeOy, 20, lEyeH, 9);

        // 2. Mắt phải gườm gườm tức giận
        int rEyeH = max(2, (int)(24 * blinkFactor));
        int rEyeY = 18 + (24 - rEyeH) / 2;
        drawEye(68 + eyeOx, rEyeY + eyeOy, 23, rEyeH, 9);

        // 3. Miệng to nhỏ co thắt liên tục như muốn chửi bới (Mouth muttering/yelling)
        int mutterDelta = (int)(abs(sin(millis() * 0.028)) * 6); // Há to ngậm nhỏ liên tục
        int curMh = max(2, 6 + mutterDelta + mouthExtraH);
        int curMw = max(4, 24 + (mutterDelta / 2));
        display.drawRBox(52 + eyeOx / 2 - mutterDelta / 4, 52 - curMh / 2, curMw, curMh, 2);
      }
      break;

    case EMO_SAD: // Buồn bã 😭 (Mắt Mochi cụp rưng rưng + Nước mắt rơi tí tách)
      {
        // 1. Mắt cụp buồn rưng rưng (chuẩn hình thái Mochi cao 24px)
        int curH = max(4, (int)(24 * blinkFactor));
        int curY = 22 + (24 - curH) / 2;
        drawEye(32 + eyeOx, curY + eyeOy, 24, curH, 10);
        drawEye(72 + eyeOx, curY + eyeOy, 24, curH, 10);

        // 2. Nước mắt rơi tí tách từ mí dưới mắt
        drawTears(44 + eyeOx, 84 + eyeOx, 46 + eyeOy);

        // 3. Miệng mếu
        display.drawRBox(58 + eyeOx / 2, 56 + 2, 12, 4, 1);
      }
      break;

    case EMO_DIZZY: // Chóng mặt 🌀 (Mắt xoắn ốc quay tròn)
      // Mắt xoắn ốc trái & phải quay ngược chiều nhau
      drawSpiral(40 + eyeOx, 26 + eyeOy, -1, spiralAngle);
      drawSpiral(88 + eyeOx, 26 + eyeOy, 1, spiralAngle);
      spiralAngle += 0.25;
      // Miệng tròn O ngơ ngác
      display.drawCircle(58 + eyeOx / 2, 53, 5);
      break;

    case EMO_SLEEPY: // SLEEPY 
      {
        int baseH = isYawning ? 6 : 8;
        int curH = max(2, (int)(baseH * blinkFactor));
        int curY = 34 + (8 - curH) / 2 + (isYawning ? 2 : 0);
        drawEye(28 + eyeOx, curY + eyeOy, 24, curH, 3);
        drawEye(76 + eyeOx, curY + eyeOy, 24, curH, 3);
      }
      // Miệng (kèm hiệu ứng há to khi ngáp)
      display.drawRBox(60 + eyeOx / 2, 53 - mouthExtraH / 2, 8, 4 + mouthExtraH, 1);
      break;

    case EMO_DEEP_SLEEP: // Ngủ say 💤 (Mắt nhắm nghiền + Miệng thở ngáy dãi chảy + 3 chữ Z bay lên)
      {
        // 1. Mắt nhắm nghiền khép dẹt (3px)
        drawEye(28 + eyeOx, 38 + eyeOy, 24, 3, 1);
        drawEye(76 + eyeOx, 38 + eyeOy, 24, 3, 1);

        // 2. Miệng thở ngáy há to ngậm nhỏ theo nhịp thở chậm
        float snoreTime = millis() * 0.0028f;
        float snoreProg = (sin(snoreTime) + 1.0f) * 0.5f; // 0.0 -> 1.0
        int curMh = max(3, (int)(6 * (0.45f + snoreProg * 1.1f)));
        int curMw = max(6, (int)(10 * (0.75f + snoreProg * 0.4f)));
        int curMy = 52 - curMh / 2;
        int curMx = 59 + eyeOx / 2 + (10 - curMw) / 2;
        display.drawRBox(curMx, curMy, curMw, curMh, min(curMw, curMh) / 2);

        // 3. Sợi nước dãi thòng xuống ở khóe miệng
        int droolX = curMx + curMw - 1;
        int droolY = curMy + curMh / 2;
        int droolLen = 4 + (int)(snoreProg * 2.5f);
        display.drawLine(droolX, droolY, droolX, droolY + droolLen);
        display.drawDisc(droolX, droolY + droolLen, 1);

        // 4. Ba chữ Z (z, Z, Z) bay lên rồi biến mất tuần hoàn
        unsigned long nowMs = millis();
        for (int i = 0; i < 3; i++) {
          unsigned long zPhase = (nowMs + i * 800) % 2400;
          float prog = (float)zPhase / 2400.0f;
          int zSize = 4 + i * 2;
          int zX = 88 + eyeOx + (int)(prog * 18.0f);
          int zY = 35 + eyeOy - (int)(prog * 22.0f);
          if (zY > 4 && zX < 122 && prog < 0.9f) {
            drawZ(zX, zY, zSize);
          }
        }
      }
      break;

    case EMO_CONFUSED: // Ngơ ngác 😳 (Soi 4 góc -> Phóng to dí mắt vào góc như bị nhốt -> Soi 4 cạnh miệng hẹp rồi to ngạc nhiên)
      {
        unsigned long ct = millis() % 7600;
        int midX = 64;
        int midY = 32;
        float confScale = 1.0f;
        int confOx = 0;
        int confOy = 0;
        float mouthScaleW = 1.0f;
        float mouthScaleH = 1.0f;
        float curBlink = blinkFactor;

        if (ct < 2000) {
          // Phase 1 (0 -> 2000ms): Soi 4 góc & 4 hướng, miệng chúm chím hẹp ngơ ngác
          confScale = 1.0f;
          mouthScaleW = 0.65f;
          mouthScaleH = 0.75f;
          if (ct < 500) { confOx = -18; confOy = -8; }
          else if (ct < 1000) { confOx = 18; confOy = -8; }
          else if (ct < 1500) { confOx = 16; confOy = 8; }
          else { confOx = -16; confOy = 8; }
        } else if (ct < 4600) {
          // Phase 2 (2000 -> 4600ms): Dí mắt vào từng góc + Phóng to hoạt ảnh 1.68x
          confScale = 1.68f;
          mouthScaleW = 1.35f;
          mouthScaleH = 1.95f;
          unsigned long tSub = ct - 2000;
          if (tSub < 650) { confOx = -24; confOy = -13; }
          else if (tSub < 1300) { confOx = 24; confOy = -13; }
          else if (tSub < 1950) { confOx = 22; confOy = 12; }
          else { confOx = -22; confOy = 12; }
        } else if (ct < 6400) {
          // Phase 3 (4600 -> 6400ms): Lùi ra soi 4 cạnh
          confScale = 0.95f;
          unsigned long tEdge = ct - 4600;
          int edgeIdx = tEdge / 450;
          int subMs = tEdge % 450;
          if (subMs < 200) {
            mouthScaleW = 0.45f; mouthScaleH = 0.55f;
          } else {
            mouthScaleW = 1.30f; mouthScaleH = 1.70f;
          }
          if (edgeIdx == 0) { confOx = -24; confOy = 0; }
          else if (edgeIdx == 1) { confOx = 0; confOy = -13; }
          else if (edgeIdx == 2) { confOx = 24; confOy = 0; }
          else { confOx = 0; confOy = 12; }
        } else {
          // Phase 4 (6400 -> 7600ms): Về tâm ngơ ngác, chớp mắt 2 nhát
          confScale = 1.0f;
          confOx = 0; confOy = 0;
          mouthScaleW = 0.75f; mouthScaleH = 1.15f;
          if ((ct >= 6700 && ct <= 6850) || (ct >= 7150 && ct <= 7300)) {
            curBlink = 0.08f;
          }
        }

        int curLw = max(4, (int)(23 * confScale));
        int curLh = max(2, (int)(38 * confScale * curBlink));
        int curLx = midX + (int)((29 - midX) * confScale) + confOx;
        int curLy = midY + (int)((7 - midY) * confScale) + confOy;
        curLx = constrain(curLx, 0, 128 - curLw);
        curLy = constrain(curLy, 0, 64 - curLh);
        drawEye(curLx, curLy, curLw, curLh, min(curLw, curLh) / 2);

        int curRw = max(4, (int)(24 * confScale));
        int curRh = max(2, (int)(40 * confScale * curBlink));
        int curRx = midX + (int)((76 - midX) * confScale) + confOx;
        int curRy = midY + (int)((6 - midY) * confScale) + confOy;
        curRx = constrain(curRx, 0, 128 - curRw);
        curRy = constrain(curRy, 0, 64 - curRh);
        drawEye(curRx, curRy, curRw, curRh, min(curRw, curRh) / 2);

        int curMw = max(4, (int)(13 * confScale * mouthScaleW));
        int curMh = max(3, (int)(6 * confScale * mouthScaleH));
        int curMx = midX + (int)((58 - midX) * confScale) + confOx;
        int curMy = midY + (int)((52 - midY) * confScale) + confOy;
        curMx = constrain(curMx, 0, 128 - curMw);
        curMy = constrain(curMy, 0, 64 - curMh);
        display.drawRBox(curMx, curMy, curMw, curMh, min(curMw, curMh) / 2);
      }
      break;

    case EMO_SURPRISED: // SURPRISED 
      {
        int baseH = isYawning ? 20 : 44;
        int curH = max(2, (int)(baseH * blinkFactor));
        int curY = -2 + (44 - curH) / 2 + (isYawning ? 9 : 0);
        drawEye(30 + eyeOx, curY + eyeOy, 20, curH, 9);
        drawEye(78 + eyeOx, curY + eyeOy, 20, curH, 9);
      }
      // Miệng (kèm hiệu ứng há to khi ngáp)
      display.drawRBox(59 + eyeOx / 2, 49 - mouthExtraH / 2, 11, 13 + mouthExtraH, 4);
      break;

    case EMO_WINK: // Nháy mắt 😉
      // Mắt trái mở to (kèm chớp nhẹ tự nhiên)
      {
        int curH = max(2, (int)(40 * blinkFactor));
        int curY = 6 + (40 - curH) / 2;
        drawEye(28 + eyeOx, curY + eyeOy, 24, curH, 10);
      }
      // Mắt phải nháy dẹt tinh nghịch
      drawEye(75 + eyeOx, 28 + eyeOy, 26, 4, 1);
      // Miệng cười
      display.drawRBox(54 + eyeOx / 2, 52, 21, 6, 2);
      break;

    case EMO_CAT: // CAT 
      {
        int baseH = isYawning ? 18 : 40;
        int curH = max(2, (int)(baseH * blinkFactor));
        int curY = 6 + (40 - curH) / 2 + (isYawning ? 8 : 0);
        drawEye(27 + eyeOx, curY + eyeOy, 26, curH, 11);
        drawEye(75 + eyeOx, curY + eyeOy, 26, curH, 11);
      }
      // Miệng (kèm hiệu ứng há to khi ngáp)
      display.drawRBox(54 + eyeOx / 2, 52 - mouthExtraH / 2, 21, 6 + mouthExtraH, 2);
      break;

    case EMO_SMILE: // SMILE 
      ffSmile(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 64 + eyeOx, 55, 16, 4, blinkFactor);
      ffOn();
      break;

    case EMO_LAUGH: // LAUGH 
      ffLaugh(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 64 + eyeOx, 55, 18, 4, blinkFactor);
      ffOn();
      break;

    case EMO_SMUG: // SMUG 
      ffSmug(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 28, 40, 64 + eyeOx, 55, 20, 4, blinkFactor);
      ffOn();
      break;

    case EMO_STAR_EYES: // STAR_EYES 
      ffStarEyes(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 64 + eyeOx, 55, 16, 4, blinkFactor);
      ffOn();
      break;

    case EMO_COOL: // COOL 
      ffCool(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 65 + eyeOx, 55, 19, 4, blinkFactor);
      ffOn();
      break;

    case EMO_CHEEKY: // CHEEKY 
      ffCheeky(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 64 + eyeOx, 55, 16, 4, blinkFactor);
      ffOn();
      break;

    case EMO_DERP: // DERP 
      ffDerp(40 + eyeOx, 26 + eyeOy, 88 + eyeOx, 26 + eyeOy, 24, 40, 64 + eyeOx, 55, 16, 4, blinkFactor);
      ffOn();
      break;
  }

  if (flush) display.sendBuffer();
}

// 6. KHỞI ĐỘNG (SETUP)

}
