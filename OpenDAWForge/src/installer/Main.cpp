#include "Installation.h"
#include <iostream>
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge::installer
{
juce::File payload()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getSiblingFile("share")
        .getChildFile("lsfactory");
}
class InstallerPanel final : public juce::Component
{
  public:
    InstallerPanel()
    {
        setSize(640, 380);
        title_.setText("LS Factory installieren", juce::dontSendNotification);
        title_.setFont(juce::Font(juce::FontOptions(26)));
        detail_.setText("Installation fuer deinen Benutzer, ohne Root-Rechte.\nLS Factory erhaelt einen "
                        "Starter und einen Eintrag im Anwendungsmenue.",
                        juce::dontSendNotification);
        detail_.setJustificationType(juce::Justification::topLeft);
        path_.setText(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile(".local/opt/lsfactory")
                          .getFullPathName());
        path_.setReadOnly(true);
        choose_.setButtonText("Ordner waehlen");
        install_.setButtonText("Installieren");
        close_.setButtonText("Schliessen");
        launch_.setButtonText("LS Factory starten");
        launch_.setVisible(false);
        shortcut_.setButtonText("Desktop-Verknuepfung erstellen");
        status_.setText("Version " FORGE_INSTALLER_VERSION, juce::dontSendNotification);
        for (auto *c : std::initializer_list<juce::Component *>{&title_, &detail_, &path_, &choose_,
                                                                &install_, &close_, &shortcut_, &status_})
            addAndMakeVisible(c);
        addChildComponent(launch_);
        choose_.onClick = [this]
        {
            chooser_ = std::make_unique<juce::FileChooser>("Installationsordner", juce::File(path_.getText()),
                                                           "", true);
            const juce::Component::SafePointer<InstallerPanel> self(this);
            chooser_->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [self](const juce::FileChooser &chooser)
                {
                    if (self && chooser.getResult() != juce::File{})
                        self->path_.setText(chooser.getResult().getChildFile("LSFactory").getFullPathName());
                });
        };
        close_.onClick = [] { juce::JUCEApplication::getInstance()->quit(); };
        launch_.onClick = [this]
        {
            if (!launcher_.startAsProcess())
                status_.setText("Der Programmstarter konnte nicht gestartet werden.",
                                juce::dontSendNotification);
        };
        install_.onClick = [this]
        {
            install_.setEnabled(false);
            choose_.setEnabled(false);
            close_.setEnabled(false);
            status_.setText("Dateien werden geprueft und installiert...", juce::dontSendNotification);
            const Options options{juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                  juce::File(path_.getText()), payload(), shortcut_.getToggleState()};
            const juce::Component::SafePointer<InstallerPanel> self(this);
            worker_.addJob(
                [self, options]
                {
                    const auto result = install(options);
                    juce::MessageManager::callAsync(
                        [self, result]
                        {
                            if (!self)
                                return;
                            self->close_.setEnabled(true);
                            self->install_.setEnabled(result.result.failed());
                            self->choose_.setEnabled(result.result.failed());
                            if (result.result.wasOk())
                            {
                                self->launcher_ = result.launcher;
                                self->status_.setText("Installation abgeschlossen. Projekte und "
                                                      "Einstellungen bleiben erhalten.",
                                                      juce::dontSendNotification);
                                self->launch_.setVisible(true);
                            }
                            else
                                self->status_.setText(result.result.getErrorMessage(),
                                                      juce::dontSendNotification);
                        });
                });
        };
    }
    ~InstallerPanel() override
    {
        worker_.removeAllJobs(true, -1);
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0xff283238));
        g.setColour(juce::Colour(0xffc4dc80));
        g.fillRect(0, 0, getWidth(), 4);
    }
    void resized() override
    {
        title_.setBounds(28, 24, 584, 40);
        detail_.setBounds(28, 80, 584, 56);
        path_.setBounds(28, 154, 416, 30);
        choose_.setBounds(458, 154, 154, 30);
        shortcut_.setBounds(28, 200, 500, 28);
        status_.setBounds(28, 242, 584, 60);
        install_.setBounds(28, 324, 150, 32);
        launch_.setBounds(192, 324, 230, 32);
        close_.setBounds(462, 324, 150, 32);
    }

  private:
    juce::Label title_, detail_, status_;
    juce::TextEditor path_;
    juce::TextButton choose_, install_, close_, launch_;
    juce::ToggleButton shortcut_;
    juce::ThreadPool worker_{1};
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::File launcher_;
};
class Window final : public juce::DocumentWindow
{
  public:
    Window() : DocumentWindow("LS Factory Setup", juce::Colour(0xff283238), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new InstallerPanel, true);
        centreWithSize(640, 380);
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->quit();
    }
};
class InstallerApplication final : public juce::JUCEApplication, private juce::Timer
{
  public:
    const juce::String getApplicationName() override
    {
        return "LS Factory Setup";
    }
    const juce::String getApplicationVersion() override
    {
        return FORGE_INSTALLER_VERSION;
    }
    void initialise(const juce::String &commandLine) override
    {
        auto args = juce::StringArray::fromTokens(commandLine, true);
        for (auto &arg : args)
            arg = arg.unquoted();
        if (args.size() >= 3 && args[0] == "--self-test")
        {
            const bool ok = selfTest(juce::File(args[1]), juce::File(args[2]));
            std::cout << (ok ? "Installer install / update / preservation / checksum checks passed"
                             : "Installer self-test failed")
                      << std::endl;
            setApplicationReturnValue(ok ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 2 && args[0] == "--install-prefix")
        {
            Options options{juce::File(args[1]).getChildFile("home"),
                            juce::File(args[1]).getChildFile("program"), payload(), false};
            const auto result = install(options);
            std::cout << (result.result.wasOk() ? "Installation completed"
                                                : result.result.getErrorMessage().toStdString())
                      << std::endl;
            setApplicationReturnValue(result.result.wasOk() ? 0 : 1);
            quit();
            return;
        }
        window_ = std::make_unique<Window>();
        if (args.size() >= 2 && args[0] == "--screenshot")
        {
            screenshot_ = juce::File(args[1]);
            startTimer(150);
        }
    }
    void shutdown() override
    {
        window_.reset();
    }

  private:
    std::unique_ptr<Window> window_;
    juce::File screenshot_;
    void timerCallback() override
    {
        stopTimer();
        const auto image = window_->getContentComponent()->createComponentSnapshot(
            window_->getContentComponent()->getLocalBounds());
        juce::FileOutputStream stream(screenshot_);
        stream.setPosition(0);
        stream.truncate();
        const bool ok = juce::PNGImageFormat().writeImageToStream(image, stream);
        setApplicationReturnValue(ok ? 0 : 1);
        quit();
    }
};
} // namespace forge::installer
START_JUCE_APPLICATION(forge::installer::InstallerApplication)
