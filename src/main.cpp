#include <QApplication>
#include "ui/MainWindow.h"
#include "core/BackupEngine.h"
#include "interfaces/IWatcher.h"
#include "interfaces/IUploader.h"
#include "interfaces/IDelta.h"

// ── Заглушки — участники 3/4/5 заменят своими реализациями ──────────────────

struct StubWatcher : backup::IWatcher {
    EventCallback cb;
    std::vector<std::string> paths;

    bool addPath(const std::string& p) override {
        paths.push_back(p); return true;
    }
    bool removePath(const std::string& p) override {
        paths.erase(std::remove(paths.begin(), paths.end(), p), paths.end());
        return true;
    }
    void setCallback(EventCallback c) override { cb = c; }
    void start() override {}
    void stop()  override {}
    std::vector<std::string> watchedPaths() const override { return paths; }
};

struct StubUploader : backup::IUploader {
    bool authenticated = false;
    bool authenticate() override { authenticated = true; return true; }
    bool isAuthenticated() const override { return authenticated; }
    bool uploadFile(const std::string&, const std::string&,
                    ProgressCallback cb) override {
        // Симулируем загрузку
        for (int i = 0; i <= 100; i += 10) {
            if (cb) cb({ "", (uint64_t)i, 100 });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return true;
    }
    bool deleteFile(const std::string&) override { return true; }
    bool createFolder(const std::string&) override { return true; }
    std::optional<std::string> getQuotaInfo() override { return "15 GB free"; }
};

struct StubDelta : backup::IDelta {
    bool hasChanged(const std::string&, const std::string&) const override {
        return true;
    }
    backup::DeltaResult prepare(const std::string& path) override {
        return { true, 1024, 512, "abc123", {} };
    }
    std::string computeHash(const std::string&) const override {
        return "abc123";
    }
};

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Network Backup");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("YourTeam");
    // Не выходить при закрытии MainWindow — остаёмся в трее
    app.setQuitOnLastWindowClosed(false);

    auto watcher  = std::make_shared<StubWatcher>();
    auto uploader = std::make_shared<StubUploader>();
    auto delta    = std::make_shared<StubDelta>();
    auto engine   = std::make_shared<backup::BackupEngine>(watcher, uploader, delta);

    backup::MainWindow window(engine);
    window.show();

    return app.exec();
}
