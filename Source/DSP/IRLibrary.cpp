#include "IRLibrary.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace IRLibrary
{
    const std::vector<Entry>& getCatalog()
    {
        static const std::vector<Entry> catalog = {
            // Residential
            { "Arroyo House Living Room",     "Residential", "Residential/Arroyo House Living Room.wav" },
            { "College House Bedroom",        "Residential", "Residential/College House Bedroom.wav" },
            { "College House Master Bathroom","Residential", "Residential/College House Master Bathroom.wav" },
            { "Old Home Living Room",         "Residential", "Residential/Old Home Living Room.wav" },
            { "Ranch House Bathroom",         "Residential", "Residential/Ranch House Bathroom.wav" },
            { "Colonial House Bedroom",       "Residential", "Residential/Colonial House Bedroom.wav" },

            // Commercial
            { "Royal Ballroom",               "Commercial", "Commercial/Royal Ballroom.wav" },
            { "Lux Hotel Bathroom",           "Commercial", "Commercial/Lux Hotel Bathroom.wav" },
            { "Firenze Parking Garage",       "Commercial", "Commercial/Firenze Parking Garage.wav" },
            { "Craft Coffee Shop",            "Commercial", "Commercial/Craft Coffee Shop.wav" },
            { "Tall Coffee Shop Lobby",       "Commercial", "Commercial/Tall Coffee Shop Lobby.wav" },

            // Public
            { "Giant Center Arena",           "Public", "Public/Giant Center Arena.wav" },
            { "UCF Engineering Atrium",       "Public", "Public/UCF Engineering Atrium.wav" },
            { "UCF Audio Club Room",          "Public", "Public/UCF Audio Club Room.wav" },
            { "Phoenix Convention Center Hall","Public", "Public/Phoenix Convention Center Hall.wav" },

            // Historical
            { "The Pantheon (Rome)",          "Historical", "Historical/The Pantheon Rome.wav" },
            { "Drumheller's Little Church",   "Historical", "Historical/Drumhellers Little Church.wav" },
            { "Sunnybrook Willowdale Church", "Historical", "Historical/Sunnybrook Willowdale Church.wav" },
            { "Menorcan Civil War Bunker",    "Historical", "Historical/Menorcan Civil War Bunker.wav" },

            // Textures (bonus, non-room)
            { "HG Spring",                    "Textures", "Textures/HG Spring.wav" },
            { "Letter Holding Coil Spring",   "Textures", "Textures/Letter Holding Coil Spring.wav" },
            { "Conner Plate Impact",          "Textures", "Textures/Conner Plate Impact.wav" },

            // --- Second batch (appended -- do not reorder, indices above are load-bearing for
            // saved presets/automation on band{N}_irIndex) ---

            // Residential
            { "13th Century Venetian Home",   "Residential", "Residential/13th Century Venetian Home.wav" },
            { "Villa 10 Hall",                "Residential", "Residential/Villa 10 Hall.wav" },
            { "Old Home Fireplace",           "Residential", "Residential/Old Home Fireplace.wav" },
            { "Ranch House Hall",             "Residential", "Residential/Ranch House Hall.wav" },

            // Commercial
            { "Palm Ballroom",                "Commercial", "Commercial/Palm Ballroom.wav" },
            { "Reflective Half Bathroom",     "Commercial", "Commercial/Reflective Half Bathroom.wav" },
            { "Hotel Guest Room",             "Commercial", "Commercial/Hotel Guest Room.wav" },
            { "Ancient Wand Shop",            "Commercial", "Commercial/Ancient Wand Shop.wav" },

            // Public
            { "MSU Arena Court Seating",      "Public", "Public/MSU Arena Court Seating.wav" },
            { "Fort Worth Convention Arena",  "Public", "Public/Fort Worth Convention Arena.wav" },
            { "OCCC Freight Elevator",        "Public", "Public/OCCC Freight Elevator.wav" },
            { "Weston's Backrooms Hall",      "Public", "Public/Westons Backrooms Hall.wav" },

            // Historical
            { "Menorcan Bunker Stairwell",    "Historical", "Historical/Menorcan Bunker Stairwell.wav" },

            // Outdoors (new category)
            { "Ostia Antica Theatre",         "Outdoors", "Outdoors/Ostia Antica Theatre.wav" },
            { "Forest Clearing",              "Outdoors", "Outdoors/Forest Clearing.wav" },
            { "Rockefeller Gardens Field Stage","Outdoors", "Outdoors/Rockefeller Gardens Field Stage.wav" },
            { "Bridge Underpass",             "Outdoors", "Outdoors/Bridge Underpass.wav" },
            { "Suburb Street",                "Outdoors", "Outdoors/Suburb Street.wav" },
            { "University Garage Stairwell",  "Outdoors", "Outdoors/University Garage Stairwell.wav" },
            { "Eerie Loading Dock",           "Outdoors", "Outdoors/Eerie Loading Dock.wav" },
            { "Rome Apartment Alley",         "Outdoors", "Outdoors/Rome Apartment Alley.wav" },

            // Textures (bonus, non-room)
            { "Car Interior",                 "Textures", "Textures/Car Interior.wav" },
            { "Storm Drain",                  "Textures", "Textures/Storm Drain.wav" },
            { "Catamaran Hull",               "Textures", "Textures/Catamaran Hull.wav" },
        };

        jassert((int) catalog.size() == catalogSize);
        return catalog;
    }

    juce::File resolveIRRoot()
    {
        // 1) Next to the running binary (plugin or standalone), under Resources/IRs -- this is
        //    where a future installer/packaging step would place factory content.
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        auto besideExe = exeDir.getChildFile("Resources").getChildFile("IRs");
        if (besideExe.isDirectory())
            return besideExe;

        // 2) Dev-build fallback: walk up from the executable looking for the source tree's
        //    Resources/IRs (works for both the Standalone app and the VST3, wherever CMake puts them).
        auto probe = exeDir;
        for (int i = 0; i < 8 && probe.exists(); ++i)
        {
            auto candidate = probe.getChildFile("Resources").getChildFile("IRs");
            if (candidate.isDirectory())
                return candidate;
            probe = probe.getParentDirectory();
        }

        // 3) User content-pack location (for a future installer).
        auto userDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                           .getChildFile("MultibandConvolver").getChildFile("IRs");
        if (userDir.isDirectory())
            return userDir;

        return {};
    }

    bool loadEntry(int index, double targetSampleRate, juce::AudioBuffer<float>& outBuffer)
    {
        outBuffer.setSize(0, 0);

        const auto& catalog = getCatalog();
        if (index < 0 || index >= (int) catalog.size())
            return false;

        auto root = resolveIRRoot();
        if (! root.isDirectory())
            return false;

        auto file = root.getChildFile(catalog[(size_t) index].relativePath);
        if (! file.existsAsFile())
            return false;

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            return false;

        const int numChannels = (int) reader->numChannels;
        const int numSamples = (int) reader->lengthInSamples;
        if (numChannels <= 0 || numSamples <= 0)
            return false;

        juce::AudioBuffer<float> raw(numChannels, numSamples);
        reader->read(&raw, 0, numSamples, 0, true, true);

        if (std::abs(reader->sampleRate - targetSampleRate) < 0.5)
        {
            outBuffer = std::move(raw);
            return true;
        }

        // Resample to the host's current sample rate.
        const double ratio = reader->sampleRate / targetSampleRate;
        const int resampledLength = (int) std::ceil((double) numSamples / ratio) + 1;
        outBuffer.setSize(numChannels, resampledLength);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            const int written = interpolator.process(ratio,
                                                       raw.getReadPointer(ch),
                                                       outBuffer.getWritePointer(ch),
                                                       resampledLength);
            outBuffer.setSize(numChannels, written, true, false, true);
        }

        return true;
    }
}
