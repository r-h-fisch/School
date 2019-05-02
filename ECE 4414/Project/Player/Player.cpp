#if ARDUINO < 100
#include <WProgram.h>
#else  // ARDUINO
#include <Arduino.h>
#endif  // ARDUINO

#include "Player.h"

// Logging
void sdErrorCheck(const SdReader& card)
{
  if (!card.errorCode()) return;
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

void error_P(const char *str, const SdReader& card)
{
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck(card);
  while(1);
}

#define error(msg, card) error_P(PSTR(msg), card)


// Player:
Player::Player()
{
  if (!Serial)
    Serial.begin(115200);

  // int free_ram = FreeRam();

  if (!card.init())
    error("Card init. failed!", card);  // Something went wrong, lets print out why

  card.partialBlockRead(true);

  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;
  }
  if (part == 5)
    error("No valid FAT partition!", card);

  // Try to open the root directory
  if (!root.openRoot(vol))
    error("Can't open root dir!", card);

  for (int i = 0; i < CHANNEL_COUNT; ++i)
    channels[i] = new File();
}

bool Player::play(const char* const filename)
{
  for (int i = 0; i < CHANNEL_COUNT; ++i)
  {
    if (!channels[i]->active)
    {
      channels[i]->active = true;
      channels[i]->file = find_and_load(filename);
      channels[i]->index = channel_top;
      channel_top += 1;
      channels[i]->sound = new Sound();

      return channels[i]->sound->play(*channels[i]->file);
    }
  }

  return false; // No free channels
  // TODO : Make this kick out the oldest sound instead
}

bool Player::play(short index)
{
  for (int i = 0; i < CHANNEL_COUNT; ++i)
    if (channels[i]->index == index)
      return channels[i]->sound->play(*channels[i]->file);
  return false;  // No such index
}

// void Player::mute()
// {
//   wave.pause();
// }

// bool Player::is_playing() const
// {
//   return wave.isplaying;
// }

bool Player::has_error() const
{
  if (!card.errorCode())
    return false;
  
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);

  return true;
}

// const uint8_t* Player::load(const char* const filename) const
// {
//     return 0;
// }

FatReader* _find_and_load(const char* const filename, FatReader* current_path, FatVolume& vol, const SdReader& card)
{
  FatReader* current_file = new FatReader();  // Current file
  dir_t dirBuf;  // Read buffer

  // Read every file in the directory one at a time
  while (current_path->readDir(dirBuf) > 0)
  {
    // Skip it if not a subdirectory and not the file we're looking for
    if (!DIR_IS_SUBDIR(dirBuf) && strncmp((char *)dirBuf.name, filename, 11))
      continue;

    // Try to open the file
    if (!current_file->open(vol, dirBuf))
      error("file.open failed", card);

    // Recurse if subdir, otherwise return the file
    if (current_file->isDir())
    {
      FatReader* temp = _find_and_load(filename, current_file, vol, card);
      if (temp != nullptr)
        return temp;
    }
    else
      return current_file;
  }

  delete current_file;
  return nullptr;
}

FatReader* Player::find_and_load(const char* const filename)
{
  return _find_and_load(filename, &root, vol, card);
}
