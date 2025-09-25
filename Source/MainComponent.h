/*
  ==============================================================================

    MainComponent.h
    Author:      Gemini

    Header file for the main UI component. It declares the class, its methods,
    and member variables.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::Component
{
public:
    //==============================================================================
    MainComponent();

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // --- Private Methods ---
    void displayIxmlFromFile(const juce::File& file);
    void openFile();

    // --- Member Variables ---
    juce::TextButton openButton;
    juce::TextEditor xmlDisplay;
    std::unique_ptr<juce::FileChooser> fileChooser;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
