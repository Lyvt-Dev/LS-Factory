#include "Updater.h"
#include <array>
#include <iostream>
#if JUCE_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif
namespace forge
{
namespace
{
int currentPid()
{
#if JUCE_WINDOWS
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}
constexpr auto repository = "https://github.com/Lyvt-Dev/LS-Factory";
constexpr auto feed = "https://api.github.com/repos/Lyvt-Dev/LS-Factory/releases?per_page=20";
std::optional<std::array<int, 3>> version(juce::String value)
{
    if (value.startsWithChar('v'))
        value = value.substring(1);
    const auto parts = juce::StringArray::fromTokens(value, ".", "");
    if (parts.size() != 3)
        return {};
    std::array<int, 3> result{};
    for (int i = 0; i < 3; ++i)
    {
        if (parts[i].isEmpty() || parts[i].length() > 6 || !parts[i].containsOnly("0123456789"))
            return {};
        result[static_cast<size_t>(i)] = parts[i].getIntValue();
    }
    return result;
}
juce::String suffix()
{
#if JUCE_WINDOWS
    return "-windows-x64-Setup.exe";
#elif JUCE_LINUX
    return "-linux-x64-Installer.AppImage";
#else
    return {};
#endif
}
std::unique_ptr<juce::InputStream> stream(const juce::String &url, int &status)
{
    return juce::URL(url).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(15000)
            .withNumRedirectsToFollow(5)
            .withStatusCode(&status)
            .withExtraHeaders("User-Agent: LSFactory-Updater\r\nAccept: application/vnd.github+json\r\n"));
}
juce::File installedDirectory()
{
#if JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
#elif JUCE_LINUX
    const auto image = juce::SystemStats::getEnvironmentVariable("APPIMAGE", {});
    if (image.isEmpty())
        return {};
    const juce::File file(image);
    const auto directory = file.getParentDirectory();
    const auto record = juce::JSON::parse(directory.getChildFile("installation.json"));
    if (file.getFileName() == "LSFactory.AppImage" &&
        record["application"].toString() == "dev.lsfactory.installer" &&
        record["destination"].toString() == directory.getFullPathName())
        return directory;
#endif
    return {};
}
} // namespace
juce::Result UpdateService::selectRelease(const juce::var &data, const juce::String &current,
                                          UpdateRelease &out)
{
    out = {};
    const auto baseline = version(current);
    const auto *releases = data.getArray();
    if (!baseline || !releases || releases->size() > 100)
        return juce::Result::fail("Ungueltige Update-Antwort.");
    if (suffix().isEmpty())
        return juce::Result::fail("Auf dieser Plattform bitte ueber die Release-Seite aktualisieren.");
    auto best = *baseline;
    for (const auto &entry : *releases)
    {
        if (static_cast<bool>(entry["draft"]))
            continue;
        const auto tag = entry["tag_name"].toString();
        const auto candidate = version(tag);
        if (!candidate || !tag.startsWithChar('v') || *candidate <= best)
            continue;
        const auto *assets = entry["assets"].getArray();
        if (!assets)
            continue;
        const auto name = "LSFactory-" + tag.substring(1) + suffix();
        const auto expected = juce::String(repository) + "/releases/download/" + tag + "/" + name;
        for (const auto &asset : *assets)
        {
            const auto digest = asset["digest"].toString();
            const auto size = static_cast<juce::int64>(asset["size"]);
            if (asset["name"].toString() != name || asset["browser_download_url"].toString() != expected ||
                !digest.startsWith("sha256:") || digest.length() != 71 ||
                !digest.substring(7).containsOnly("0123456789abcdef") || size <= 0 ||
                size > 128 * 1024 * 1024)
                continue;
            out = {tag.substring(1), name, expected, digest.substring(7), size};
            best = *candidate;
            break;
        }
    }
    return juce::Result::ok();
}
juce::Result UpdateService::check(const juce::String &current, UpdateRelease &out)
{
    int status = 0;
    auto input = stream(feed, status);
    if (!input || status != 200)
        return juce::Result::fail("Update-Pruefung fehlgeschlagen (Netzwerk/GitHub HTTP " +
                                  juce::String(status) + "). Erneut versuchen.");
    juce::MemoryBlock bytes;
    std::array<char, 8192> buffer{};
    for (;;)
    {
        const auto n = input->read(buffer.data(), static_cast<int>(buffer.size()));
        if (n < 0)
            return juce::Result::fail("Update-Antwort konnte nicht gelesen werden.");
        if (n == 0)
            break;
        if (bytes.getSize() + static_cast<size_t>(n) > 2 * 1024 * 1024)
            return juce::Result::fail("Update-Antwort zu gross.");
        bytes.append(buffer.data(), static_cast<size_t>(n));
    }
    juce::var data;
    auto result = juce::JSON::parse(
        juce::String::fromUTF8(static_cast<const char *>(bytes.getData()), static_cast<int>(bytes.getSize())),
        data);
    return result.failed() ? juce::Result::fail("Ungueltige Update-Antwort.")
                           : selectRelease(data, current, out);
}
juce::Result UpdateService::download(const UpdateRelease &release, const juce::File &directory,
                                     juce::File &out)
{
    out = juce::File{};
    const auto valid = version(release.version);
    const auto expected = juce::String(repository) + "/releases/download/v" + release.version +
                          "/LSFactory-" + release.version + suffix();
    if (!valid || release.url != expected || release.filename != "LSFactory-" + release.version + suffix() ||
        release.sha256.length() != 64 || !release.sha256.containsOnly("0123456789abcdef") ||
        release.size <= 0 || release.size > 128 * 1024 * 1024)
        return juce::Result::fail("Ungueltige Update-Datei.");
    if (auto r = directory.createDirectory(); r.failed())
        return r;
    const auto partial = directory.getChildFile("download-" + juce::Uuid().toString() + ".part");
    struct Cleanup
    {
        juce::File file;
        ~Cleanup()
        {
            file.deleteFile();
        }
    } cleanup{partial};
    int status = 0;
    auto input = stream(release.url, status);
    if (!input || status != 200)
        return juce::Result::fail("Download fehlgeschlagen (HTTP " + juce::String(status) + ").");
    {
        juce::FileOutputStream output(partial);
        if (output.failedToOpen())
            return output.getStatus();
        std::array<char, 65536> buffer{};
        juce::int64 total = 0;
        for (;;)
        {
            const auto n = input->read(buffer.data(), static_cast<int>(buffer.size()));
            if (n < 0)
                return juce::Result::fail("Download unterbrochen.");
            if (n == 0)
                break;
            total += n;
            if (total > release.size || !output.write(buffer.data(), static_cast<size_t>(n)))
                return juce::Result::fail("Download ungueltig oder Speicher voll.");
        }
        output.flush();
        if (output.getStatus().failed() || total != release.size)
            return juce::Result::fail("Download ist unvollstaendig.");
    }
    if (juce::SHA256(partial).toHexString() != release.sha256)
        return juce::Result::fail("SHA-256-Pruefung fehlgeschlagen. Update wurde nicht gestartet.");
    const auto target = directory.getChildFile(release.filename);
    if (!partial.moveFileTo(target))
        return juce::Result::fail("Update-Datei konnte nicht gespeichert werden.");
#if JUCE_LINUX
    if (!target.setExecutePermission(true))
        return juce::Result::fail("Update-Datei konnte nicht ausfuehrbar gemacht werden.");
#endif
    out = target;
    return juce::Result::ok();
}
juce::Result UpdateService::verifyFile(const UpdateRelease &release, const juce::File &file)
{
    if (!file.existsAsFile() || file.getSize() != release.size ||
        juce::SHA256(file).toHexString() != release.sha256)
        return juce::Result::fail(
            "Update-Datei wurde veraendert oder ist unvollstaendig. Bitte erneut pruefen und herunterladen.");
    return juce::Result::ok();
}
bool UpdateService::smokeTest(const juce::File &directory)
{
    if (directory.createDirectory().failed())
        return false;
    auto fixture = [](const juce::String &v, bool draft = false)
    {
        auto asset = std::make_unique<juce::DynamicObject>();
        const auto name = "LSFactory-" + v + suffix();
        asset->setProperty("name", name);
        asset->setProperty("browser_download_url",
                           juce::String(repository) + "/releases/download/v" + v + "/" + name);
        asset->setProperty("digest", "sha256:" + juce::String::repeatedString("a", 64));
        asset->setProperty("size", 16);
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty("tag_name", "v" + v);
        entry->setProperty("draft", draft);
        entry->setProperty("assets", juce::var(juce::Array<juce::var>{juce::var(asset.release())}));
        return juce::var(entry.release());
    };
    auto older = fixture("1.2.9"), newer = fixture("1.10.0"), draft = fixture("2.0.0", true);
    UpdateRelease selected;
    if (selectRelease(juce::var(juce::Array<juce::var>{older, draft, newer}), "1.2.10", selected).failed() ||
        selected.version != "1.10.0")
        return false;
    if (selectRelease(juce::var(juce::Array<juce::var>{older, newer}), "1.10.0", selected).failed() ||
        selected.version.isNotEmpty())
        return false;
    newer["assets"][0].getDynamicObject()->setProperty("browser_download_url",
                                                       "https://example.com/untrusted.exe");
    if (selectRelease(juce::var(juce::Array<juce::var>{newer}), "0.0.0", selected).failed() ||
        selected.version.isNotEmpty())
        return false;
    if (selectRelease(juce::var("malformed"), "0.0.0", selected).wasOk())
        return false;
    UpdateRelease release;
    const auto checked = check("0.0.0", release);
    if (checked.failed() || release.version.isEmpty())
    {
        std::cerr << checked.getErrorMessage() << std::endl;
        return false;
    }
    juce::File file;
    const auto downloaded = download(release, directory, file);
    if (downloaded.failed())
    {
        std::cerr << downloaded.getErrorMessage() << std::endl;
        return false;
    }
    if (verifyFile(release, file).failed())
        return false;
    const auto corrupt = directory.getChildFile("tampered-update-test");
    corrupt.replaceWithText("tampered");
    if (verifyFile(release, corrupt).wasOk())
        return false;
    corrupt.deleteFile();
    auto untrusted = release;
    untrusted.url = "https://example.com/untrusted.exe";
    if (download(untrusted, directory, file).wasOk())
        return false;
    std::cout
        << "Updater version selection / trusted metadata / verified download / corruption rejection passed"
        << std::endl;
    return true;
}
UpdatePanel::UpdatePanel(std::function<bool()> ready, bool automaticChecks, juce::File preferences)
    : canInstall_(std::move(ready)),
      preferences_(preferences == juce::File{}
                       ? juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("LSFactory/update-settings.json")
                       : preferences),
      automaticChecks_(automaticChecks)
{
    setSize(680, 370);
    for (auto *c :
         std::initializer_list<juce::Component *>{&title_, &status_, &checkAtStart_, &installAutomatically_,
                                                  &checkNow_, &installNow_, &releasePage_})
        addAndMakeVisible(c);
    title_.setText("Programm-Updates", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(24)));
    status_.setText("Version " + juce::JUCEApplication::getInstance()->getApplicationVersion() +
                        ". Updates kommen aus dem offiziellen GitHub-Release.",
                    juce::dontSendNotification);
    const auto settings = juce::JSON::parse(preferences_);
    checkAtStart_.setToggleState(
        settings.hasProperty("checkAtStart") ? static_cast<bool>(settings["checkAtStart"]) : true,
        juce::dontSendNotification);
    installAutomatically_.setToggleState(static_cast<bool>(settings["installAutomatically"]),
                                         juce::dontSendNotification);
    installAutomatically_.setEnabled(!suffix().isEmpty());
    installNow_.setEnabled(false);
    checkAtStart_.onClick = [this]
    {
        if (!checkAtStart_.getToggleState())
            installAutomatically_.setToggleState(false, juce::dontSendNotification);
        saveSettings();
    };
    installAutomatically_.onClick = [this]
    {
        if (installAutomatically_.getToggleState())
            checkAtStart_.setToggleState(true, juce::dontSendNotification);
        saveSettings();
    };
    checkNow_.onClick = [this] { check(); };
    installNow_.onClick = [this]
    {
        if (installer_.existsAsFile())
            apply();
        else
            download();
    };
    releasePage_.onClick = [] { juce::URL(juce::String(repository) + "/releases").launchInDefaultBrowser(); };
    startTimer(1000);
}
UpdatePanel::~UpdatePanel()
{
    stopTimer();
    worker_.removeAllJobs(true, -1);
}
bool UpdatePanel::preferencesSmokeTest()
{
    if (automaticChecks_)
        return false;
    installAutomatically_.setToggleState(true, juce::dontSendNotification);
    installAutomatically_.onClick();
    const auto enabled = juce::JSON::parse(preferences_);
    if (!static_cast<bool>(enabled["installAutomatically"]) || !static_cast<bool>(enabled["checkAtStart"]))
        return false;
    UpdatePanel restored([] { return false; }, false, preferences_);
    if (!restored.installAutomatically_.getToggleState() || !restored.checkAtStart_.getToggleState())
        return false;
    checkAtStart_.setToggleState(false, juce::dontSendNotification);
    checkAtStart_.onClick();
    const auto disabled = juce::JSON::parse(preferences_);
    if (static_cast<bool>(disabled["installAutomatically"]) || static_cast<bool>(disabled["checkAtStart"]))
        return false;
    checkAtStart_.setToggleState(true, juce::dontSendNotification);
    checkAtStart_.onClick();
    return true;
}
void UpdatePanel::saveSettings()
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("checkAtStart", checkAtStart_.getToggleState());
    object->setProperty("installAutomatically", installAutomatically_.getToggleState());
    if (preferences_.getParentDirectory().createDirectory().failed() ||
        !preferences_.replaceWithText(juce::JSON::toString(juce::var(object.release()))))
        status_.setText("Update-Einstellungen konnten nicht gespeichert werden.", juce::dontSendNotification);
}
void UpdatePanel::timerCallback()
{
    if (!started_)
    {
        started_ = true;
        if (automaticChecks_ && checkAtStart_.getToggleState())
            check();
    }
    if (automaticChecks_ && installAutomatically_.getToggleState() && !busy_ && release_.version.isNotEmpty())
    {
        if (installer_.existsAsFile())
        {
            if (canInstall_())
                apply();
        }
        else if (!autoAttempted_ && installedDirectory() != juce::File{})
            download();
    }
}
void UpdatePanel::check()
{
    if (busy_)
        return;
    autoAttempted_ = false;
    busy_ = true;
    checkNow_.setEnabled(false);
    installNow_.setEnabled(false);
    status_.setText("Suche nach Updates...", juce::dontSendNotification);
    const juce::Component::SafePointer<UpdatePanel> self(this);
    const auto current = juce::JUCEApplication::getInstance()->getApplicationVersion();
    worker_.addJob(
        [self, current]
        {
            UpdateRelease release;
            const auto result = UpdateService::check(current, release);
            juce::MessageManager::callAsync(
                [self, result, release]
                {
                    if (!self)
                        return;
                    self->busy_ = false;
                    self->checkNow_.setEnabled(true);
                    self->release_ = release;
                    self->installer_ = juce::File{};
                    self->installNow_.setEnabled(result.wasOk() && release.version.isNotEmpty());
                    self->status_.setText(
                        result.failed() ? result.getErrorMessage()
                        : release.version.isEmpty()
                            ? "Keine neuere verifizierbare Installer-Version gefunden."
                            : "Version " + release.version +
                                  " verfuegbar. Update vor Installation mit SHA-256 pruefen.",
                        juce::dontSendNotification);
                });
        });
}
void UpdatePanel::download()
{
    if (busy_ || release_.version.isEmpty())
        return;
    autoAttempted_ = true;
    busy_ = true;
    installNow_.setEnabled(false);
    checkNow_.setEnabled(false);
    status_.setText("Update wird heruntergeladen und geprueft...", juce::dontSendNotification);
    const auto release = release_;
    const auto directory = preferences_.getParentDirectory().getChildFile("updates/" + release.version);
    const juce::Component::SafePointer<UpdatePanel> self(this);
    worker_.addJob(
        [self, release, directory]
        {
            juce::File installer;
            const auto result = UpdateService::download(release, directory, installer);
            juce::MessageManager::callAsync(
                [self, result, installer]
                {
                    if (!self)
                        return;
                    self->busy_ = false;
                    self->checkNow_.setEnabled(true);
                    self->installNow_.setEnabled(true);
                    if (result.failed())
                    {
                        self->status_.setText(result.getErrorMessage(), juce::dontSendNotification);
                        return;
                    }
                    self->installer_ = installer;
                    self->apply();
                });
        });
}
void UpdatePanel::apply()
{
    if (!installer_.existsAsFile())
        return;
    if (auto r = UpdateService::verifyFile(release_, installer_); r.failed())
    {
        status_.setText(r.getErrorMessage(), juce::dontSendNotification);
        installer_ = juce::File{};
        release_.version.clear();
        installNow_.setEnabled(false);
        return;
    }
    if (!canInstall_())
    {
        status_.setText("Update bereit. Bitte Projekt speichern und Wiedergabe/Aufnahme stoppen. Dann Update "
                        "installieren.",
                        juce::dontSendNotification);
        return;
    }
    const auto destination = installedDirectory();
    juce::StringArray args{installer_.getFullPathName()};
#if JUCE_WINDOWS
    args.addArray({"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LSFACTORYUPDATE=1",
                   "/WAITPID=" + juce::String(currentPid()), "/DIR=" + destination.getFullPathName()});
#elif JUCE_LINUX
    if (destination == juce::File{})
    {
        status_.setText(
            "Automatisches Update benoetigt eine Installer-Installation. Bitte die Release-Seite verwenden.",
            juce::dontSendNotification);
        return;
    }
    args.addArray({"--appimage-extract-and-run", "--update-install", destination.getFullPathName(),
                   "--wait-pid", juce::String(currentPid())});
#else
    return;
#endif
    // Argument arrays bypass shell expansion, including installation paths with spaces.
    juce::ChildProcess process;
    if (!process.start(args, 0))
    {
        status_.setText("Update-Installer konnte nicht gestartet werden.", juce::dontSendNotification);
        return;
    }
    juce::JUCEApplication::getInstance()->quit();
}
void UpdatePanel::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff303a40));
}
void UpdatePanel::resized()
{
    auto area = getLocalBounds().reduced(24);
    title_.setBounds(area.removeFromTop(38));
    area.removeFromTop(16);
    checkAtStart_.setBounds(area.removeFromTop(30));
    installAutomatically_.setBounds(area.removeFromTop(52));
    area.removeFromTop(14);
    status_.setBounds(area.removeFromTop(90));
    area.removeFromTop(14);
    auto row = area.removeFromTop(32);
    checkNow_.setBounds(row.removeFromLeft(140));
    row.removeFromLeft(12);
    installNow_.setBounds(row.removeFromLeft(180));
    row.removeFromLeft(12);
    releasePage_.setBounds(row.removeFromLeft(150));
}
} // namespace forge
