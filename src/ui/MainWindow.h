#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <memory>
#include "../core/BackupEngine.h"

namespace backup {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<BackupEngine> engine,
                        QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onAddFolder();
    void onAddFiles();
    void onRemovePath();
    void onStartStop();
    void onPauseResume();
    void onFullBackup();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void refreshStats();

private:
    void buildUi();
    void buildTray();
    void updatePathList();
    void applyStats(const BackupStats& stats);
    QString formatBytes(uint64_t bytes) const;
    QString statusText(BackupStatus s) const;

    std::shared_ptr<BackupEngine> m_engine;

    // ─── Widgets ───────────────────────────────────────────────────────
    QListWidget* m_pathList       = nullptr;
    QPushButton* m_btnAddFolder   = nullptr;
    QPushButton* m_btnAddFiles    = nullptr;
    QPushButton* m_btnRemove      = nullptr;
    QPushButton* m_btnStartStop   = nullptr;
    QPushButton* m_btnPause       = nullptr;
    QPushButton* m_btnFullBackup  = nullptr;

    QProgressBar* m_progress      = nullptr;
    QLabel*       m_lblStatus     = nullptr;
    QLabel*       m_lblCurrent    = nullptr;
    QLabel*       m_lblStats      = nullptr;

    // ─── Tray ──────────────────────────────────────────────────────────
    QSystemTrayIcon* m_tray       = nullptr;
    QMenu*           m_trayMenu   = nullptr;

    // ─── Polling timer ─────────────────────────────────────────────────
    QTimer* m_statsTimer          = nullptr;
    bool    m_isRunning           = false;
    bool    m_isPaused            = false;
};

} // namespace backup
