#include "about_controller.h"

#include <apps/apps_container.h>
#include <apps/exam_mode_manager.h>
#include <assert.h>
#include <ion/device_name.h>
#include <ion/external_apps.h>
#include <ion/src/shared/events.h>
#include <ion/storage/file_system.h>

#include <cmath>

using namespace Escher;

namespace Settings {

AboutController::AboutController(Responder* parentResponder)
    : GenericSubController(parentResponder),
#ifdef HARDWARE_CHECKUP
      m_hardwareCheckupPopUpController(
          Escher::Invocation(
              [](void* context, void* sender) {
                AppsContainer* appsContainer =
                    AppsContainer::sharedAppsContainer();
                appsContainer->switchToBuiltinApp(
                    appsContainer->hardwareCheckupAppSnapshot());
                return true;
              },
              this),
          I18n::Message::HardwareCheckupLaunch),
#endif
      m_hardwareTestPopUpController(
          Escher::Invocation(
              [](void* context, void* sender) {
                AppsContainer* appsContainer =
                    AppsContainer::sharedAppsContainer();
                appsContainer->switchToBuiltinApp(
                    appsContainer->hardwareTestAppSnapshot());
                return true;
              },
              this),
          I18n::Message::HardwareTestLaunch) {
}

#ifdef HARDWARE_CHECKUP
static bool IsHardwareCheckupRow(int row) {
  return row == AboutController::Row(AboutController::CellType::Version);
}
#endif

bool AboutController::handleEvent(Ion::Events::Event event) {
  if (m_messageTreeModel->label() == I18n::Message::About &&
      !ExamModeManager::ExamMode().isActive()) {
    /* We hide here the activation for the hardware tests.
     * In the menu "About", on any cell,
     * - press '6' for default tests
     * - press '1' for variant (tests all keys on keyboard)
     * Also prevent hardware test in exam mode so that the LED can't be
     * accessed. */
    bool isDefaultKey = event == Ion::Events::Six ||
                        event == Ion::Events::LowerT ||
                        event == Ion::Events::UpperT;
    bool isVariantKey = event == Ion::Events::One ||
                        event == Ion::Events::LowerW ||
                        event == Ion::Events::UpperW;
    if (isDefaultKey || isVariantKey) {
      AppsContainer::sharedAppsContainer()->setHardwareTestVariant(
          isDefaultKey ? HardwareTest::Variant::Default
                       : HardwareTest::Variant::TestAllKeys);
      m_hardwareTestPopUpController.presentModally();
      return true;
    }
  }
#ifdef HARDWARE_CHECKUP
  /* We hide here the activation of the hardware checkup app: in the menu
   * "about", by clicking on '7' on the hardware checkup row. */
  if (event == Ion::Events::Seven &&
      m_messageTreeModel->label() == I18n::Message::About &&
      IsHardwareCheckupRow(selectedRow()) &&
      !ExamModeManager::ExamMode().isActive()) {
    // Prevent hardware checkup in exam mode.
    m_hardwareCheckupPopUpController.presentModally();
    return true;
  }
#endif
  if (event == Ion::Events::OK || event == Ion::Events::EXE) {
    if (selectedRow() == Row(CellType::Version)) {
      /* When pressing OK on the version cell, the display cycles between
       * Epsilon version number, the commit hash for this build of Epsilon, the
       * PCB revision number, the flags used at compilation and the bootloader
       * running on the device. */
      assert(m_selectableListView.selectedCell() ==
             m_cells + Row(CellType::Version));

      using TextGetter = const char* (*)();
      constexpr TextGetter k_textGettersCycle[] = {
          &Ion::softwareVersion, &Ion::commitHash, &Ion::pcbVersion,
          &Ion::compilationFlags, &Ion::runningBootloader};
      constexpr int k_nGetters = std::size(k_textGettersCycle);

      const char* previousText =
          m_cells[Row(CellType::Version)].subLabel()->text();
      for (int i = 0; i < k_nGetters; i++) {
        if (strcmp(previousText, k_textGettersCycle[i]()) == 0) {
          m_cells[Row(CellType::Version)].subLabel()->setText(
              k_textGettersCycle[(i + 1) % k_nGetters]());
          // Reload frame with new text length
          m_selectableListView.reloadSelectedCell(true);
          return true;
        }
      }
      assert(false);  // Text not found in cycle
    }
#if TERMS_OF_USE
    if (selectedRow() == Row(CellType::TermsOfUse)) {
      Ion::Events::openURL(I18n::translate(I18n::Message::TermsOfUseLink));
      return true;
    }
#endif
    return false;
  }
  return GenericSubController::handleEvent(event);
}

HighlightCell* AboutController::reusableCell(int index, int type) {
  assert(type == 0);
  assert(index >= 0 && index < k_totalNumberOfCell);
  return &m_cells[index];
}

KDCoordinate AboutController::nonMemoizedRowHeight(int row) {
  return m_cells[row].isVisible()
             ? m_cells[row].minimalSizeForOptimalDisplay().height()
             : 0;
}

static char* AppendNumber(char* position, int value) {
  if (value >= 10) {
    position = AppendNumber(position, value / 10);
  }
  *position = static_cast<char>('0' + value % 10);
  return position + 1;
}

/* Writes "used/total KB" in the buffer. The sizes are rounded to the nearest
 * KB. */
static void WriteUsage(char* buffer, int usedBytes, int totalBytes) {
  constexpr int k_bytesPerKB = 1024;
  if (usedBytes < 0) {
    usedBytes = 0;
  }
  char* position = buffer;
  position =
      AppendNumber(position, (usedBytes + k_bytesPerKB / 2) / k_bytesPerKB);
  *position++ = '/';
  position =
      AppendNumber(position, (totalBytes + k_bytesPerKB / 2) / k_bytesPerKB);
  *position++ = ' ';
  *position++ = 'K';
  *position++ = 'B';
  *position = '\0';
}

void AboutController::updateStorageText() {
  // Storage of the calculator data (scripts, functions...)
  int totalBytes = static_cast<int>(Ion::Storage::FileSystem::k_totalSize);
  int availableBytes = static_cast<int>(
      Ion::Storage::FileSystem::sharedFileSystem->availableSize());
  WriteUsage(m_storageText, totalBytes - availableBytes, totalBytes);

  // Flash memory reserved for the external apps
  uint32_t appsUsedBytes = 0;
  uint32_t appsTotalBytes = 0;
  Ion::ExternalApps::flashUsage(&appsUsedBytes, &appsTotalBytes);
  WriteUsage(m_appsText, static_cast<int>(appsUsedBytes),
             static_cast<int>(appsTotalBytes));
}

void AboutController::viewWillAppear() {
  updateStorageText();
  const char* messages[k_totalNumberOfCell] = {
    deviceName(),
    Ion::softwareVersion(),
    Ion::serialNumber(),
    Ion::fccId(),
    m_storageText,
    m_appsText,
#if TERMS_OF_USE
    "",
#endif
  };
  for (int i = 0; i < k_totalNumberOfCell; i++) {
    m_cells[i].subLabel()->setText(messages[i]);
  }
  if (strlen(deviceName()) == 0) {
    m_cells[Row(CellType::DeviceName)].setVisible(false);
  }
  if (strncmp(Ion::fccId(), "NA", 3) == 0) {
    m_cells[Row(CellType::FCCID)].setVisible(false);
  }
  GenericSubController::viewWillAppear();
}

const char* AboutController::deviceName() const {
  return Ion::DeviceName::read();
}

}  // namespace Settings