#include "engine/NoteStringParser.h"

#include <cctype>
#include <stdexcept>
#include <string>

#include "engine/Theory.h"

namespace pmg {

namespace {

std::vector<std::string> Tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    size_t i = 0;
    size_t n = s.size();
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        size_t start = i;
        while (i < n && !std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        tokens.push_back(s.substr(start, i - start));
    }
    return tokens;
}

bool IsNoteLetter(char c) {
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return upper >= 'A' && upper <= 'G';
}

double ParseDuration(const std::string& text, const std::string& token) {
    if (text.empty()) {
        throw std::runtime_error("NoteStringParser: malformed duration in token '" + token + "'");
    }
    double value = 0.0;
    size_t consumed = 0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error("NoteStringParser: malformed duration in token '" + token + "'");
    }
    if (consumed != text.size()) {
        throw std::runtime_error("NoteStringParser: malformed duration in token '" + token + "'");
    }
    if (!(value > 0.0)) {
        throw std::runtime_error("NoteStringParser: duration must be greater than zero in token '" + token + "'");
    }
    return value;
}

} // namespace

std::vector<StepConfig> ParseNoteString(const MelodyConfig& melody) {
    std::vector<StepConfig> steps;
    std::vector<std::string> tokens = Tokenize(melody.notes);

    double cursor = melody.startBeat;

    for (const std::string& token : tokens) {
        char first = token[0];
        bool isRest = (first == 'R' || first == 'r');

        double durationBeats = melody.noteLengthBeats;
        bool emitStep = false;
        NoteName note = NoteName::C;
        int octave = melody.defaultOctave;

        if (isRest) {
            size_t pos = 1;
            if (pos < token.size() && token[pos] == ':') {
                durationBeats = ParseDuration(token.substr(pos + 1), token);
                pos = token.size();
            }
            if (pos != token.size()) {
                throw std::runtime_error(
                    "NoteStringParser: malformed token '" + token + "' (rest cannot carry pitch/octave)");
            }
        } else if (IsNoteLetter(first)) {
            emitStep = true;
            std::string noteName(1, static_cast<char>(std::toupper(static_cast<unsigned char>(first))));
            size_t pos = 1;

            if (pos < token.size() && (token[pos] == '#' || token[pos] == 'b')) {
                noteName += token[pos];
                ++pos;
            }

            if (pos < token.size() && std::isdigit(static_cast<unsigned char>(token[pos]))) {
                octave = token[pos] - '0';
                ++pos;
            }

            if (pos < token.size() && token[pos] == ':') {
                durationBeats = ParseDuration(token.substr(pos + 1), token);
                pos = token.size();
            }

            if (pos != token.size()) {
                throw std::runtime_error("NoteStringParser: malformed token '" + token +
                                          "' (unexpected trailing '" + token.substr(pos) + "')");
            }

            try {
                note = Theory::ParseNoteName(noteName);
            } catch (const std::exception& e) {
                throw std::runtime_error("NoteStringParser: malformed token '" + token + "': " + e.what());
            }
        } else {
            throw std::runtime_error(
                "NoteStringParser: malformed token '" + token + "' (expected a note letter A-G or rest 'R')");
        }

        if (emitStep) {
            StepConfig step;
            step.beat = cursor;
            step.instrument = melody.instrument;
            step.hasNote = true;
            step.note = note;
            step.octave = octave;
            step.velocity = melody.velocity;
            step.gate = static_cast<float>(durationBeats * melody.gateFraction);
            steps.push_back(step);
        }

        cursor += durationBeats;
    }

    return steps;
}

} // namespace pmg
