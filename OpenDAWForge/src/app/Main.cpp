#include "MainWindow.h"
#include "Updater.h"
#include "persistence/JsonExporter.h"
#include <iostream>
namespace forge
{
class FactoryApplication final : public juce::JUCEApplication, private juce::Timer
{
  public:
    const juce::String getApplicationName() override
    {
        return "LS Factory";
    }
    const juce::String getApplicationVersion() override
    {
        return "0.4.2";
    }
    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }
    void initialise(const juce::String &commandLine) override
    {
        auto args = juce::StringArray::fromTokens(commandLine, true);
        args.removeEmptyStrings();
        for (auto &argument : args)
            argument = argument.unquoted();
        if (args.size() >= 3 && args[0] == "--scan-plugin")
        {
            const auto result = PluginManager::scanInProcess(juce::File(args[1]), juce::File(args[2]));
            std::cout << (result ? "Plugin scanned" : result.error) << std::endl;
            setApplicationReturnValue(result ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 3 && args[0] == "--plugin-smoke-test")
        {
            const auto directory = juce::File(args[2]);
            directory.createDirectory();
            auto engine = std::make_unique<AudioEngine>();
            const bool result = engine->pluginSmokeTest(juce::File(args[1]), directory);
            std::cout << (result ? "VST3 scan / processing / bypass / state roundtrip passed"
                                 : "VST3 smoke failed")
                      << std::endl;
            setApplicationReturnValue(result ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 2 && args[0] == "--factory-smoke-test")
        {
            const bool ok = library::Library::smokeTest(juce::File(args[1]));
            setApplicationReturnValue(ok ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 2 && args[0] == "--update-smoke-test")
        {
            const bool ok = UpdateService::smokeTest(juce::File(args[1]));
            setApplicationReturnValue(ok ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 2 && args[0] == "--export-json")
        {
            const auto result = exportJson(makeDemoProject(), args[1].toStdString());
            setApplicationReturnValue(result ? 0 : 1);
            quit();
            return;
        }
        if (args.size() >= 2 && args[0] == "--render-demo")
        {
            auto engine = std::make_unique<AudioEngine>();
            const auto result = engine->exportWave(makeDemoProject(), juce::File(args[1]));
            std::cout << (result ? "WAV rendered" : result.error) << std::endl;
            setApplicationReturnValue(result ? 0 : 1);
            quit();
            return;
        }
        smoke_ = args.size() >= 2 && args[0] == "--smoke-test";
        if (smoke_)
        {
            output_ = juce::File(args[1]);
            output_.createDirectory();
        }
        window_ = std::make_unique<MainWindow>(!smoke_);
        if (args.size() == 1 && juce::File(args[0]).hasFileExtension("odaw"))
            window_->workspace().openProject(juce::File(args[0]));
        if (smoke_)
            startTimer(200);
    }
    void shutdown() override
    {
        stopTimer();
        window_.reset();
    }
    void systemRequestedQuit() override
    {
        quit();
    }
    void anotherInstanceStarted(const juce::String &) override {}

  private:
    std::unique_ptr<MainWindow> window_;
    bool smoke_ = false;
    juce::File output_;
    void timerCallback() override
    {
        stopTimer();
        const bool success = window_->workspace().smokeTest(output_);
        std::cout << (success ? "UI / project / WAV smoke passed" : "Smoke failed") << std::endl;
        setApplicationReturnValue(success ? 0 : 1);
        quit();
    }
};
} // namespace forge
START_JUCE_APPLICATION(forge::FactoryApplication)
