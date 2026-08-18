#include "book_of_answers_answers.h"

namespace {

constexpr BookMessageAnswer kMessageAnswers[] = {
    {"TRUST YOUR", "INSTINCTS"},
    {"TAKE THE", "NEXT STEP"},
    {"GIVE IT", "MORE TIME"},
    {"ASK AGAIN", "TOMORROW"},
    {"LET IT", "UNFOLD"},
    {"LOOK", "CLOSER"},
    {"THE TIMING", "IS RIGHT"},
    {"KEEP", "GOING"},
    {"CHANGE YOUR", "APPROACH"},
    {"REST", "FIRST"},
    {"SAY WHAT", "YOU MEAN"},
    {"FOLLOW THE", "QUIET CLUE"},
    {"DO LESS", "BUT BETTER"},
    {"MAKE SPACE", "FOR THE NEW"},
    {"YOU ALREADY", "KNOW"},
    {"BE", "PATIENT"},
    {"CHOOSE THE", "KIND PATH"},
    {"START", "SMALL"},
};

constexpr const char *kCrystalAnswers[] = {
    "YES",
    "NO",
    "UNCLEAR",
};

}  // namespace

size_t book_message_answer_count()
{
    return sizeof(kMessageAnswers) / sizeof(kMessageAnswers[0]);
}

const BookMessageAnswer &book_message_answer(size_t index)
{
    return kMessageAnswers[index % book_message_answer_count()];
}

size_t book_crystal_answer_count()
{
    return sizeof(kCrystalAnswers) / sizeof(kCrystalAnswers[0]);
}

const char *book_crystal_answer(size_t index)
{
    return kCrystalAnswers[index % book_crystal_answer_count()];
}
