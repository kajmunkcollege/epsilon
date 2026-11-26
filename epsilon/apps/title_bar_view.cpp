#include "title_bar_view.h"

#include <escher/theme/color_picker.h>
#include <exam_icon.h>
#include <omg/unreachable.h>
#include <poincare/print.h>

#include <array>

#include "exam_mode_manager.h"
#include "global_preferences.h"

extern "C" {
#include <assert.h>
}

using namespace Poincare;
using namespace Escher;

/* Rough battery percentage estimated from the battery voltage (linear between
 * these two values). Tune them if the percentage feels off. */
constexpr static float k_batteryEmptyVoltage = 3.5f;
constexpr static float k_batteryFullVoltage = 4.15f;

TitleBarView::TitleBarView()
    : View(),
      m_titleView(I18n::Message::Default, k_glyphsFormat),
      m_preferenceView(k_glyphsFormat),
      m_examModeIconView(*ImageStore::ExamIcon),
      m_examModeTextView(I18n::Message::Default, k_glyphsFormat),
      m_batteryPercentView(k_glyphsFormat) {
  m_preferenceView.setAlignment(KDGlyph::k_alignRight, KDGlyph::k_alignCenter);
  m_batteryPercentView.setAlignment(KDGlyph::k_alignRight,
                                    KDGlyph::k_alignCenter);
}

void TitleBarView::drawRect(KDContext* ctx, KDRect rect) const {
  /* As we cheated to layout the title view, we have to fill a very thin
   * rectangle at the top with the background color. */
  ctx->fillRect(KDRect(0, 0, bounds().width(), k_verticalShift),
                ColorPicker::Resolve(k_backgroundColor));
}

void TitleBarView::setTitle(I18n::Message title) {
  m_titleView.setMessage(title);
}

bool TitleBarView::setChargeState(Ion::Battery::Charge chargeState) {
  bool iconChanged = m_batteryView.setChargeState(chargeState);
  bool percentChanged = refreshBatteryPercent();
  return iconChanged || percentChanged;
}

bool TitleBarView::refreshBatteryPercent() {
  float percent = (Ion::Battery::voltage() - k_batteryEmptyVoltage) /
                  (k_batteryFullVoltage - k_batteryEmptyVoltage) * 100.0f;
  if (percent < 0.0f) {
    percent = 0.0f;
  } else if (percent > 100.0f) {
    percent = 100.0f;
  }
  int roundedPercent = static_cast<int>(percent + 0.5f);
  if (roundedPercent == m_batteryPercent) {
    return false;
  }
  m_batteryPercent = roundedPercent;
  char buffer[k_batteryPercentTextSize];
  int length = 0;
  if (roundedPercent >= 100) {
    buffer[length++] = '1';
    buffer[length++] = '0';
    buffer[length++] = '0';
  } else {
    if (roundedPercent >= 10) {
      buffer[length++] = '0' + roundedPercent / 10;
    }
    buffer[length++] = '0' + roundedPercent % 10;
  }
  buffer[length++] = '%';
  buffer[length] = '\0';
  m_batteryPercentView.setText(buffer);
  return true;
}

bool TitleBarView::setIsCharging(bool isCharging) {
  return m_batteryView.setIsCharging(isCharging);
}

bool TitleBarView::setIsPlugged(bool isPlugged) {
  return m_batteryView.setIsPlugged(isPlugged);
}

bool TitleBarView::setShiftAlphaLockStatus(
    Ion::Events::ShiftAlphaStatus status) {
  return m_shiftAlphaLockView.setStatus(status);
}

void TitleBarView::updateBatteryAnimation() {
  m_batteryView.updateBatteryAnimation();
}

int TitleBarView::numberOfSubviews() const { return 8; }

View* TitleBarView::subviewAtIndex(int index) {
  View* views[] = {&m_titleView,          &m_preferenceView,
                   &m_examModeIconView,   &m_examModeTextView,
                   &m_shiftAlphaLockView, &m_batteryView,
                   &m_separatorView,      &m_batteryPercentView};
  assert(0 <= index && index < static_cast<int>(std::size(views)));
  return views[index];
}

void TitleBarView::layoutSubviews(bool force) {
  setChildFrame(&m_titleView,
                KDRect(0, k_verticalShift, bounds().width(),
                       bounds().height() - k_verticalShift),
                force);
  setChildFrame(&m_preferenceView,
                KDRect(Metric::TitleBarExternHorizontalMargin, 0,
                       m_preferenceView.minimalSizeForOptimalDisplay().width(),
                       bounds().height()),
                force);
  KDSize batterySize = m_batteryView.minimalSizeForOptimalDisplay();
  KDCoordinate batteryX = bounds().width() - batterySize.width() -
                          Metric::TitleBarExternHorizontalMargin;
  setChildFrame(
      &m_batteryView,
      KDRect(batteryX, (bounds().height() - batterySize.height()) / 2,
             batterySize),
      force);
  // The battery percentage is displayed on the left of the battery icon
  KDCoordinate percentX =
      batteryX - k_batteryPercentRightMargin - k_batteryPercentWidth;
  setChildFrame(&m_batteryPercentView,
                KDRect(percentX, 0, k_batteryPercentWidth, bounds().height()),
                force);
  if (ExamModeManager::ExamMode().isActive()) {
    setChildFrame(
        &m_examModeIconView,
        KDRect(k_examIconMargin, (bounds().height() - k_examIconHeight) / 2,
               k_examIconWidth, k_examIconHeight),
        force);
    setChildFrame(&m_examModeTextView,
                  KDRect(k_examIconMargin - k_examTextWidth, k_verticalShift,
                         k_examTextWidth, bounds().height() - k_verticalShift),
                  force);
    I18n::Message examModeMessage;
    switch (ExamModeManager::ExamMode().ruleset()) {
      case ExamMode::Ruleset::English:
        examModeMessage = I18n::Message::ExamModeTitleBarUK;
        break;
      case ExamMode::Ruleset::Dutch:
        examModeMessage = I18n::Message::ExamModeTitleBarNL;
        break;
      case ExamMode::Ruleset::Portuguese:
        examModeMessage = I18n::Message::ExamModeTitleBarPT;
        break;
      case ExamMode::Ruleset::IBTest:
        examModeMessage = I18n::Message::ExamModeTitleBarIB;
        break;
      case ExamMode::Ruleset::STAAR:
        examModeMessage = I18n::Message::ExamModeTitleBarSTAAR;
        break;
      case ExamMode::Ruleset::Pennsylvania:
        examModeMessage = I18n::Message::ExamModeTitleBarPennsylvania;
        break;
      case ExamMode::Ruleset::SouthCarolina:
        examModeMessage = I18n::Message::ExamModeTitleBarSouthCarolina;
        break;
      case ExamMode::Ruleset::NorthCarolina:
        examModeMessage = I18n::Message::ExamModeTitleBarNorthCarolina;
        break;
      case ExamMode::Ruleset::SAT:
        examModeMessage = I18n::Message::ExamModeTitleBarSAT;
        break;
      default:
        examModeMessage = I18n::Message::Default;
    }
    m_examModeTextView.setMessage(examModeMessage);
  } else {
    setChildFrame(&m_examModeIconView, KDRectZero, force);
    m_examModeTextView.setMessage(I18n::Message::Default);
  }
  KDSize shiftAlphaLockSize =
      m_shiftAlphaLockView.minimalSizeForOptimalDisplay();
  setChildFrame(&m_shiftAlphaLockView,
                KDRect(percentX - k_alphaRightMargin - shiftAlphaLockSize.width(),
                       (bounds().height() - shiftAlphaLockSize.height()) / 2,
                       shiftAlphaLockSize),
                force);
  setChildFrame(&m_separatorView,
                KDRect(0, bounds().height() - Metric::CellSeparatorThickness,
                       bounds().width(), Metric::CellSeparatorThickness),
                force);
}

void TitleBarView::refreshPreferences() {
  char buffer[k_preferenceTextSize];
  // Display Sci/ or Eng/ if the print float mode is not decimal
  const Preferences::PrintFloatMode printFloatMode =
      SharedGlobalPrefs().displayMode();
  I18n::Message floatModeMessage =
      printFloatMode == Preferences::PrintFloatMode::Decimal
          ? I18n::Message::Default
          : (printFloatMode == Preferences::PrintFloatMode::Scientific
                 ? I18n::Message::Sci
                 : I18n::Message::Eng);
  // Display the angle unit
  const AngleUnit angleUnit = SharedGlobalPrefs().angleUnit();
  I18n::Message angleMessage;
  switch (angleUnit) {
    case AngleUnit::Degree:
      angleMessage = I18n::Message::Deg;
      break;
    case AngleUnit::None:
      OMG_UNREACHABLE;
      [[fallthrough]];
    case AngleUnit::Radian:
      angleMessage = I18n::Message::Rad;
      break;
    case AngleUnit::Gradian:
      angleMessage = I18n::Message::Gon;
      break;
  }
  Poincare::Print::CustomPrintf(buffer, k_preferenceTextSize, "%s%s",
                                I18n::translate(floatModeMessage),
                                I18n::translate(angleMessage));
  m_preferenceView.setText(buffer);
  // Layout the exam mode icon if needed
  layoutSubviews();
}

void TitleBarView::reload() {
  refreshPreferences();
  refreshBatteryPercent();
  markWholeFrameAsDirty();
}
