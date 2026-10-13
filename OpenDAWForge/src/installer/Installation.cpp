#include "Installation.h"
#include <iostream>
#include <unistd.h>
#include <vector>
namespace forge::installer
{
namespace
{
constexpr const char *managed = "dev.lsfactory.installer";
juce::String shellQuote(juce::String s)
{
    return "'" + s.replace("'", "'\"'\"'") + "'";
}
juce::String desktopQuote(juce::String s)
{
    return "\"" +
           s.replace("\\", "\\\\")
               .replace("\"", "\\\"")
               .replace("`", "\\`")
               .replace("$", "\\$")
               .replace("%", "%%") +
           "\"";
}
struct Replacement
{
    juce::File source, target, backup;
    bool existed = false, applied = false;
    Replacement(juce::File s, juce::File t) : source(std::move(s)), target(std::move(t)) {}
};
} // namespace
Outcome install(const Options &o)
{
    auto fail = [](juce::String s) { return Outcome{juce::Result::fail(s), {}}; };
    if (o.home.getFullPathName().containsAnyOf("\r\n") ||
        o.destination.getFullPathName().containsAnyOf("\r\n"))
        return fail("Der Installationspfad darf keine Zeilenumbrueche enthalten.");
    const auto source = o.payloadDirectory.getChildFile("LSFactory.AppImage");
    const auto expected = o.payloadDirectory.getChildFile("payload.sha256").loadFileAsString().trim();
    if (!source.existsAsFile())
        return fail("Die Programmdatei fehlt im Installer: " + source.getFullPathName());
    if (expected.length() != 64)
        return fail("Die Pruefsummendatei fehlt oder ist ungueltig: " + o.payloadDirectory.getFullPathName());
    if (juce::SHA256(source).toHexString() != expected)
        return fail("Die SHA-256-Pruefsumme der Programmdatei stimmt nicht. Bitte den Installer erneut "
                    "herunterladen.");
    const auto record = o.destination.getChildFile("installation.json");
    if (o.destination.isSymbolicLink())
        return fail("Der Installationsordner darf kein symbolischer Link sein.");
    if (o.destination.exists())
    {
        const auto prior = juce::JSON::parse(record);
        if (!o.destination.isDirectory() ||
            (!o.destination.findChildFiles(juce::File::findFilesAndDirectories, false).isEmpty() &&
             prior["application"].toString() != managed))
            return fail(
                "Dieser Ordner enthaelt andere Dateien. Bitte einen leeren Installationsordner waehlen.");
    }
    const auto bin = o.home.getChildFile(".local/bin/lsfactory"),
               menu = o.home.getChildFile(".local/share/applications/lsfactory.desktop");
    const juce::String marker = "# Managed by dev.lsfactory.installer";
    for (const auto &path : {bin, menu})
        if (path.exists() && (path.isSymbolicLink() || !path.loadFileAsString().contains(marker)))
            return fail("Eine andere Datei belegt bereits " + path.getFullPathName());
    const auto stage = o.destination.getSiblingFile(".lsfactory-install-" + juce::Uuid().toString());
    if (auto r = stage.createDirectory(); r.failed())
        return fail(r.getErrorMessage());
    bool retain = false;
    struct Cleanup
    {
        juce::File dir;
        bool &retain;
        ~Cleanup()
        {
            if (!retain)
                dir.deleteRecursively();
        }
    } cleanup{stage, retain};
    const auto stagedProgram = stage.getChildFile("LSFactory.AppImage"),
               stagedIcon = stage.getChildFile("lsfactory.svg");
    if (!source.copyFileTo(stagedProgram) || !stagedProgram.setExecutePermission(true) ||
        juce::SHA256(stagedProgram).toHexString() != expected ||
        !o.payloadDirectory.getChildFile("lsfactory.svg").copyFileTo(stagedIcon))
        return fail("Die Programmdateien konnten nicht vorbereitet werden.");
    const auto installed = o.destination.getChildFile("LSFactory.AppImage"),
               icon = o.destination.getChildFile("lsfactory.svg");
    const auto stagedLauncher = stage.getChildFile("lsfactory");
    const auto launchText = "#!/bin/sh\n" + marker + "\nexport APPIMAGE_EXTRACT_AND_RUN=1\nexec " +
                            shellQuote(installed.getFullPathName()) + " \"$@\"\n";
    if (!stagedLauncher.replaceWithText(launchText, false, false, "\n") ||
        !stagedLauncher.setExecutePermission(true))
        return fail("Der Programmstarter konnte nicht erstellt werden.");
    const auto stagedMenu = stage.getChildFile("lsfactory.desktop");
    const auto menuText =
        marker +
        "\n[Desktop Entry]\nType=Application\nName=LS Factory\nComment=Native pattern workstation\nExec=" +
        desktopQuote(bin.getFullPathName()) + " %f\nIcon=" + icon.getFullPathName() +
        "\nTerminal=false\nCategories=AudioVideo;Audio;\nMimeType=application/x-lsfactory-project;\n";
    if (!stagedMenu.replaceWithText(menuText))
        return fail("Der Menueintrag konnte nicht erstellt werden.");
    const auto stagedRecord = stage.getChildFile("installation.json");
    auto info = std::make_unique<juce::DynamicObject>();
    info->setProperty("application", managed);
    info->setProperty("version", FORGE_INSTALLER_VERSION);
    info->setProperty("destination", o.destination.getFullPathName());
    info->setProperty("sha256", expected);
    if (!stagedRecord.replaceWithText(juce::JSON::toString(juce::var(info.release()))))
        return fail("Installationsdaten konnten nicht gespeichert werden.");
    const auto desktop = o.home == juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                             ? juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                                   .getChildFile("LSFactory.desktop")
                             : o.home.getChildFile("Desktop/LSFactory.desktop");
    const auto uninstall = o.destination.getChildFile("uninstall.sh"),
               stagedUninstall = stage.getChildFile("uninstall.sh");
    const auto uninstallText =
        juce::String(
            "#!/bin/sh\nset -eu\nprintf 'LS Factory entfernen? [j/N] '\nread -r answer\ncase \"$answer\" in "
            "j|J|y|Y) ;; *) exit 0 ;; esac\n") +
        "test -f " + shellQuote(record.getFullPathName()) + "\ngrep -q 'dev.lsfactory.installer' " +
        shellQuote(record.getFullPathName()) + "\n" + "if grep -q 'Managed by dev.lsfactory.installer' " +
        shellQuote(bin.getFullPathName()) + " 2>/dev/null; then rm -f -- " +
        shellQuote(bin.getFullPathName()) + "; fi\n" + "if grep -q 'Managed by dev.lsfactory.installer' " +
        shellQuote(menu.getFullPathName()) + " 2>/dev/null; then rm -f -- " +
        shellQuote(menu.getFullPathName()) + "; fi\n" + "if grep -q 'Managed by dev.lsfactory.installer' " +
        shellQuote(desktop.getFullPathName()) + " 2>/dev/null; then rm -f -- " +
        shellQuote(desktop.getFullPathName()) + "; fi\n" + "rm -f -- " +
        shellQuote(installed.getFullPathName()) + " " + shellQuote(icon.getFullPathName()) + " " +
        shellQuote(record.getFullPathName()) + " " + shellQuote(uninstall.getFullPathName()) + "\nrmdir -- " +
        shellQuote(o.destination.getFullPathName()) +
        " 2>/dev/null || true\nprintf 'LS Factory wurde entfernt. Eigene Dateien bleiben erhalten.\\n'\n";
    if (!stagedUninstall.replaceWithText(uninstallText, false, false, "\n") ||
        !stagedUninstall.setExecutePermission(true))
        return fail("Der Deinstallationshelfer konnte nicht erstellt werden.");
    std::vector<Replacement> files{{stagedProgram, installed}, {stagedIcon, icon},
                                   {stagedRecord, record},     {stagedUninstall, uninstall},
                                   {stagedLauncher, bin},      {stagedMenu, menu}};
    if (o.desktopShortcut)
    {
        if (desktop.exists() && !desktop.loadFileAsString().contains(marker))
            return fail("Eine andere Desktop-Verknuepfung ist vorhanden.");
        files.push_back({stagedMenu, desktop});
    }
    // Back up only files managed by this installer; never remove the installation directory recursively.
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        auto &f = files[i];
        if (f.target.isSymbolicLink())
            return fail("Eine Zieldatei ist ein symbolischer Link: " + f.target.getFullPathName());
        if (f.target.exists() && !f.target.existsAsFile())
            return fail("Eine andere Datei oder ein Ordner belegt " + f.target.getFullPathName());
        f.existed = f.target.existsAsFile();
        f.backup = stage.getChildFile("backup-" + juce::String(static_cast<int>(i)));
        if (f.existed && !f.target.copyFileTo(f.backup))
            return fail("Die vorherige Installation konnte nicht gesichert werden.");
        if (auto r = f.target.getParentDirectory().createDirectory(); r.failed())
            return fail(r.getErrorMessage());
    }
    for (auto &f : files)
    {
        f.applied = true;
        const bool executable = f.target == installed || f.target == bin || f.target == uninstall;
        if (!f.source.copyFileTo(f.target) || (executable && !f.target.setExecutePermission(true)))
        {
            bool restored = true;
            for (auto &prior : files)
                if (prior.applied)
                {
                    if (prior.existed)
                        restored = prior.backup.copyFileTo(prior.target) && restored;
                    else
                        restored = prior.target.deleteFile() && restored;
                }
            if (!restored)
            {
                retain = true;
                return fail("Installation fehlgeschlagen. Sicherungen liegen unter " +
                            stage.getFullPathName());
            }
            return fail("Installation fehlgeschlagen; die vorherigen Dateien wurden wiederhergestellt.");
        }
    }
    return {juce::Result::ok(), bin};
}
bool selfTest(const juce::File &payload, const juce::File &root)
{
    if (root.exists())
        return false;
    if (root.createDirectory().failed())
        return false;
    Options options{root.getChildFile("home"), root.getChildFile("program"), payload, true};
    auto result = install(options);
    if (result.result.failed())
    {
        std::cerr << result.result.getErrorMessage() << std::endl;
        return false;
    }
    if (!options.home.getChildFile("Desktop/LSFactory.desktop").existsAsFile())
        return false;
    const auto custom = options.destination.getChildFile("my-project.txt");
    custom.replaceWithText("preserve");
    if (install(options).result.failed() || custom.loadFileAsString() != "preserve")
        return false;
    const auto protectedFolder = root.getChildFile("protected");
    protectedFolder.createDirectory();
    protectedFolder.getChildFile("user.txt").replaceWithText("keep");
    options.destination = protectedFolder;
    if (install(options).result.wasOk() ||
        protectedFolder.getChildFile("user.txt").loadFileAsString() != "keep")
        return false;
    options.destination = root.getChildFile("tamper-target");
    const auto corrupt = root.getChildFile("corrupt-payload");
    payload.copyDirectoryTo(corrupt);
    corrupt.getChildFile("payload.sha256").replaceWithText(juce::String::repeatedString("0", 64));
    options.payloadDirectory = corrupt;
    if (install(options).result.wasOk() || options.destination.exists())
        return false;
    return result.launcher.existsAsFile() &&
           ::access(result.launcher.getFullPathName().toRawUTF8(), X_OK) == 0;
}
} // namespace forge::installer
