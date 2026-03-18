#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QStyle>
#include <QFont>

namespace backup {

// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(std::shared_ptr<BackupEngine> engine, QWidget* parent)
    : QMainWindow(parent)
    , m_engine(std::move(engine))
{
    setWindowTitle("Network Backup");
    setMinimumSize(680, 520);
    resize(720, 560);

    buildUi();
    buildTray();

    // Polling stats каждые 500ms
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    m_statsTimer->start(500);

    updatePathList();
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // ── Заголовок ─────────────────────────────────────────────────────
    auto* titleLabel = new QLabel("Network Backup  →  Google Drive", this);
    QFont f = titleLabel->font();
    f.setPointSize(14);
    f.setBold(true);
    titleLabel->setFont(f);
    root->addWidget(titleLabel);

    // ── Список путей ──────────────────────────────────────────────────
    auto* grpPaths = new QGroupBox("Отслеживаемые папки и файлы", this);
    auto* vPaths   = new QVBoxLayout(grpPaths);

    m_pathList = new QListWidget(this);
    m_pathList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_pathList->setAlternatingRowColors(true);
    vPaths->addWidget(m_pathList);

    auto* hBtns = new QHBoxLayout();
    m_btnAddFolder = new QPushButton("+ Папку", this);
    m_btnAddFiles  = new QPushButton("+ Файлы", this);
    m_btnRemove    = new QPushButton("Удалить", this);
    m_btnRemove->setEnabled(false);
    hBtns->addWidget(m_btnAddFolder);
    hBtns->addWidget(m_btnAddFiles);
    hBtns->addStretch();
    hBtns->addWidget(m_btnRemove);
    vPaths->addLayout(hBtns);
    root->addWidget(grpPaths);

    connect(m_btnAddFolder, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    connect(m_btnAddFiles,  &QPushButton::clicked, this, &MainWindow::onAddFiles);
    connect(m_btnRemove,    &QPushButton::clicked, this, &MainWindow::onRemovePath);
    connect(m_pathList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_btnRemove->setEnabled(!m_pathList->selectedItems().isEmpty());
    });

    // ── Управление ────────────────────────────────────────────────────
    auto* grpCtrl = new QGroupBox("Управление", this);
    auto* hCtrl   = new QHBoxLayout(grpCtrl);

    m_btnStartStop  = new QPushButton("▶  Запустить", this);
    m_btnPause      = new QPushButton("⏸  Пауза",     this);
    m_btnFullBackup = new QPushButton("⟳  Полный бэкап", this);
    m_btnPause->setEnabled(false);
    m_btnFullBackup->setEnabled(false);

    hCtrl->addWidget(m_btnStartStop);
    hCtrl->addWidget(m_btnPause);
    hCtrl->addStretch();
    hCtrl->addWidget(m_btnFullBackup);
    root->addWidget(grpCtrl);

    connect(m_btnStartStop,  &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(m_btnPause,      &QPushButton::clicked, this, &MainWindow::onPauseResume);
    connect(m_btnFullBackup, &QPushButton::clicked, this, &MainWindow::onFullBackup);

    // ── Прогресс и статус ─────────────────────────────────────────────
    auto* grpStatus = new QGroupBox("Статус", this);
    auto* vStatus   = new QVBoxLayout(grpStatus);

    m_lblStatus = new QLabel("Остановлен", this);
    m_lblStatus->setStyleSheet("font-weight: bold;");

    m_lblCurrent = new QLabel("", this);
    m_lblCurrent->setWordWrap(true);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);

    m_lblStats = new QLabel("Загружено: 0 файлов  |  Пропущено: 0  |  Ошибок: 0  |  Трафик: 0 B", this);
    m_lblStats->setStyleSheet("color: gray; font-size: 11px;");

    vStatus->addWidget(m_lblStatus);
    vStatus->addWidget(m_lblCurrent);
    vStatus->addWidget(m_progress);
    vStatus->addWidget(m_lblStats);
    root->addWidget(grpStatus);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildTray() {
    m_tray     = new QSystemTrayIcon(this);
    m_trayMenu = new QMenu(this);

    m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_DriveNetIcon));
    m_tray->setToolTip("Network Backup");

    auto* actShow = m_trayMenu->addAction("Показать окно");
    m_trayMenu->addSeparator();
    auto* actQuit = m_trayMenu->addAction("Выход");

    connect(actShow, &QAction::triggered, this, &QWidget::showNormal);
    connect(actQuit, &QAction::triggered, qApp,  &QApplication::quit);
    connect(m_tray,  &QSystemTrayIcon::activated,
            this,    &MainWindow::onTrayActivated);

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::closeEvent(QCloseEvent* event) {
    // Сворачиваем в трей вместо выхода
    hide();
    event->ignore();
    m_tray->showMessage("Network Backup",
                        "Приложение продолжает работу в трее.",
                        QSystemTrayIcon::Information, 2000);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onAddFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Выбрать папку для отслеживания",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir.isEmpty()) return;

    if (!m_engine->addWatchPath(dir.toStdString())) {
        QMessageBox::warning(this, "Ошибка",
            "Не удалось добавить папку:\n" + dir);
        return;
    }
    updatePathList();
}

void MainWindow::onAddFiles() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Выбрать файлы для отслеживания", QDir::homePath());

    for (const auto& f : files)
        m_engine->addWatchPath(f.toStdString());

    updatePathList();
}

void MainWindow::onRemovePath() {
    for (auto* item : m_pathList->selectedItems()) {
        m_engine->removeWatchPath(item->text().toStdString());
    }
    updatePathList();
}

void MainWindow::onStartStop() {
    if (!m_isRunning) {
        if (m_engine->watchedPaths().empty()) {
            QMessageBox::information(this, "Нет путей",
                "Добавьте хотя бы одну папку или файл.");
            return;
        }
        bool ok = m_engine->start();
        if (!ok) {
            auto stats = m_engine->currentStats();
            QMessageBox::critical(this, "Ошибка запуска",
                QString::fromStdString(stats.lastError));
            return;
        }
        m_isRunning = true;
        m_btnStartStop->setText("⏹  Остановить");
        m_btnPause->setEnabled(true);
        m_btnFullBackup->setEnabled(true);
    } else {
        m_engine->stop();
        m_isRunning = false;
        m_isPaused  = false;
        m_btnStartStop->setText("▶  Запустить");
        m_btnPause->setText("⏸  Пауза");
        m_btnPause->setEnabled(false);
        m_btnFullBackup->setEnabled(false);
    }
}

void MainWindow::onPauseResume() {
    if (!m_isPaused) {
        m_engine->pause();
        m_isPaused = true;
        m_btnPause->setText("▶  Продолжить");
    } else {
        m_engine->resume();
        m_isPaused = false;
        m_btnPause->setText("⏸  Пауза");
    }
}

void MainWindow::onFullBackup() {
    m_engine->triggerFullBackup();
    m_tray->showMessage("Network Backup", "Запущен полный бэкап...",
                        QSystemTrayIcon::Information, 1500);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick)
        showNormal();
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::refreshStats() {
    applyStats(m_engine->currentStats());
}

void MainWindow::applyStats(const BackupStats& s) {
    m_lblStatus->setText(statusText(s.status));

    if (!s.currentFile.empty()) {
        QString fname = QString::fromStdString(s.currentFile);
        m_lblCurrent->setText("Загружается: " + fname);
        m_progress->setValue(static_cast<int>(s.currentFilePercent));
    } else {
        m_lblCurrent->setText("");
        m_progress->setValue(0);
    }

    m_lblStats->setText(QString("Загружено: %1 файл(ов)  |  Пропущено: %2  |  "
                                "Ошибок: %3  |  Трафик: %4")
        .arg(s.filesUploaded)
        .arg(s.filesSkipped)
        .arg(s.errors)
        .arg(formatBytes(s.bytesUploaded)));

    // Обновить иконку трея по статусу
    if (s.status == BackupStatus::Uploading)
        m_tray->setToolTip("Network Backup — загружается...");
    else if (s.status == BackupStatus::Watching)
        m_tray->setToolTip("Network Backup — слежу за файлами");
    else
        m_tray->setToolTip("Network Backup — остановлен");
}

void MainWindow::updatePathList() {
    m_pathList->clear();
    for (const auto& p : m_engine->watchedPaths())
        m_pathList->addItem(QString::fromStdString(p));
}

QString MainWindow::formatBytes(uint64_t bytes) const {
    if (bytes < 1024)       return QString("%1 B").arg(bytes);
    if (bytes < 1048576)    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1073741824) return QString("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
    return QString("%1 GB").arg(bytes / 1073741824.0, 0, 'f', 2);
}

QString MainWindow::statusText(BackupStatus s) const {
    switch (s) {
        case BackupStatus::Idle:      return "⚫  Остановлен";
        case BackupStatus::Watching:  return "🟢  Слежу за файлами";
        case BackupStatus::Uploading: return "🔵  Загружаю в Google Drive...";
        case BackupStatus::Error:     return "🔴  Ошибка";
        case BackupStatus::Paused:    return "🟡  Пауза";
    }
    return "";
}

} // namespace backup
