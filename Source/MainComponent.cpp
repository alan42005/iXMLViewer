/*
  ==============================================================================

    MainComponent.cpp
    Author:      Gemini

    This component provides a basic UI to open a WAV file, search for an
    "iXML" chunk, and display its contents.

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // --- UI Setup ---

    // Configure the "Open File" button
    addAndMakeVisible(openButton);
    openButton.setButtonText("Open WAV File...");
    openButton.onClick = [this] { openFile(); };

    // Configure the text editor for displaying the iXML content
    addAndMakeVisible(xmlDisplay);
    xmlDisplay.setMultiLine(true);
    xmlDisplay.setReadOnly(true);
    // FIXED: Use the modern JUCE Font constructor that takes FontOptions
    
    xmlDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey.darker());
    xmlDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::lightgoldenrodyellow);
    xmlDisplay.setText("Select a Broadcast WAV file to view its iXML metadata...");

    // Set the size of our main component window
    setSize(800, 600);
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // Fill the background with a solid color
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    // This is called when the MainComponent is resized.
    // We'll lay out our child components here.
    juce::Rectangle<int> area = getLocalBounds();

    int buttonHeight = 40;
    int padding = 10;

    openButton.setBounds(area.removeFromTop(buttonHeight).reduced(padding / 2));
    area.removeFromTop(padding); // Add some space
    xmlDisplay.setBounds(area.reduced(padding));
}

//==============================================================================
/* This is the core logic function to find and display the iXML chunk. */
void MainComponent::displayIxmlFromFile(const juce::File& file)
{
    xmlDisplay.clear();

    // Create a stream to read the file
    std::unique_ptr<juce::FileInputStream> inputStream(file.createInputStream());

    if (inputStream == nullptr)
    {
        xmlDisplay.setText("Error: Could not open file for reading.");
        return;
    }

    // A WAV file is a type of RIFF container. We need to manually parse it
    // to find the iXML chunk, as JUCE's audio format readers are focused on audio data.

    // Check for "RIFF" header
    char riffHeader[4];
    if (inputStream->read(riffHeader, 4) != 4 || juce::String(riffHeader, 4) != "RIFF")
    {
        xmlDisplay.setText("Error: This does not appear to be a valid RIFF (WAV) file.");
        return;
    }

    // Skip overall file size (4 bytes)
    inputStream->skipNextBytes(4);

    // Check for "WAVE" format identifier
    char waveHeader[4];
    if (inputStream->read(waveHeader, 4) != 4 || juce::String(waveHeader, 4) != "WAVE")
    {
        xmlDisplay.setText("Error: This is not a WAVE file.");
        return;
    }

    bool ixmlFound = false;
    bool bextFound = false;
    juce::String formatSummary;
    juce::String bextSummary;
    juce::String ixmlContent;

    // Loop through all the chunks in the file
    while (!inputStream->isExhausted())
    {
        char chunkId[4];
        if (inputStream->read(chunkId, 4) != 4)
            break; // End of file reached prematurely

        // Read chunk size (little-endian 32-bit integer)
        juce::int32 chunkSize = inputStream->readInt();
        if (chunkSize < 0)
        {
            xmlDisplay.setText("Error: Encountered an invalid chunk size.");
            return;
        }

        auto currentChunkId = juce::String(chunkId, 4);

        if (currentChunkId == "fmt ")
        {
            // It's the format chunk. Let's read the data.
            // The input stream is little-endian by default which is correct for WAV.
            short audioFormat = inputStream->readShort();
            short numChannels = inputStream->readShort();
            int sampleRate = inputStream->readInt();
            inputStream->skipNextBytes(4); // Skip Byte Rate
            inputStream->skipNextBytes(2); // Skip Block Align
            short bitsPerSample = inputStream->readShort();

            formatSummary << "WAV File Properties:\n";
            formatSummary << "--------------------\n";
            formatSummary << "Audio Format: " << (audioFormat == 1 ? "PCM" : "Compressed (Format ID: " + juce::String(audioFormat) + ")") << "\n";
            formatSummary << "Channels: " << juce::String(numChannels) << "\n";
            formatSummary << "Sample Rate: " << juce::String(sampleRate) << " Hz\n";
            formatSummary << "Bit Depth: " << juce::String(bitsPerSample) << " bits\n";

            // The fmt chunk can sometimes be larger than the standard 16 bytes.
            // We need to skip any extra bytes to get to the next chunk correctly.
            int bytesRead = 2 + 2 + 4 + 4 + 2 + 2;
            if (chunkSize > bytesRead)
            {
                inputStream->skipNextBytes(chunkSize - bytesRead);
            }
        }
        else if (currentChunkId == "bext")
        {
            // It's the Broadcast Wave Format (BWF) extension chunk.
            bextFound = true;

            auto readBextField = [&inputStream](int numBytes) -> juce::String
                {
                    juce::MemoryBlock temp(numBytes);
                    inputStream->read(temp.getData(), numBytes);
                    // Create a string and trim any trailing null characters
                    return juce::String::fromUTF8(static_cast<const char*>(temp.getData()), numBytes).trim();
                };

            bextSummary << "Broadcast Extension (bext) Data:\n";
            bextSummary << "----------------------------------\n";
            bextSummary << "Description: " << readBextField(256) << "\n";
            bextSummary << "Originator: " << readBextField(32) << "\n";
            bextSummary << "Originator Ref: " << readBextField(32) << "\n";
            bextSummary << "Origination Date: " << readBextField(10) << "\n";
            bextSummary << "Origination Time: " << readBextField(8) << "\n";
            bextSummary << "Time Reference: " << juce::String(inputStream->readInt64()) << " (samples since midnight)\n";

            // Skip the rest of the bext chunk to get to the next one
            // We've read 256+32+32+10+8+8 = 346 bytes.
            int bextBytesRead = 346;
            if (chunkSize > bextBytesRead)
            {
                inputStream->skipNextBytes(chunkSize - bextBytesRead);
            }
        }
        else if (currentChunkId == "iXML")
        {
            // We found it!
            ixmlFound = true;

            // Read the chunk data into memory
            juce::MemoryBlock chunkData;
            // FIXED: Correctly resize the memory block and read the data into its raw data pointer
            chunkData.setSize(chunkSize);
            if (inputStream->read(chunkData.getData(), chunkSize) == chunkSize)
            {
                // The data is XML, which is text. We can convert it to a String.
                // The iXML spec says it should be UTF-8.
                auto xmlString = juce::String::fromUTF8(static_cast<const char*>(chunkData.getData()), (int)chunkData.getSize());

                // Try to parse and pretty-print it for readability
                if (auto parsedXml = juce::XmlDocument::parse(xmlString))
                {
                    ixmlContent = parsedXml->toString();
                }
                else
                {
                    // If parsing fails, just show the raw text
                    ixmlContent = xmlString;
                }
            }
        }
        else
        {
            // This is not the chunk we're looking for. Skip over its data.
            inputStream->skipNextBytes(chunkSize);

            // RIFF chunks are padded to be an even number of bytes.
            // If chunkSize is odd, we need to skip one more padding byte.
            if (chunkSize % 2 != 0)
            {
                inputStream->skipNextBytes(1);
            }
        }
    }

    // Assemble the final string for display
    juce::String finalText;
    if (formatSummary.isNotEmpty())
    {
        finalText << formatSummary;
    }
    else
    {
        finalText << "WAV Format data (fmt chunk) not found.\n";
    }

    finalText << "\n";

    if (bextFound)
    {
        finalText << bextSummary;
    }
    else
    {
        finalText << "No Broadcast Extension (bext) chunk found in this file.\n";
    }

    finalText << "\n";

    if (ixmlFound)
    {
        finalText << "iXML Metadata:\n";
        finalText << "--------------------\n";
        finalText << ixmlContent;
    }
    else
    {
        finalText << "No iXML chunk was found in this file.";
    }

    xmlDisplay.setText(finalText);
}

/* Opens a file chooser dialog to select a WAV file. */
void MainComponent::openFile()
{
    // Use JUCE's FileChooser to create a native "open file" dialog
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a WAV file to open...",
        juce::File{},
        "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    // Launch the dialog asynchronously
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                // A file was selected, so let's process it
                displayIxmlFromFile(file);
            }
        });
}



