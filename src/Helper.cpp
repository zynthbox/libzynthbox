#include "Helper.h"

tracktion::AudioTrack *Helper::getOrInsertAudioTrackAt(
    tracktion::Edit &edit, int index) {
  edit.ensureNumberOfAudioTracks(index + 1);
  return tracktion::getAudioTracks(edit)[index];
}

void Helper::removeAllClips(tracktion::AudioTrack &track) {
  auto clips = track.getClips();

  for (int i = clips.size(); --i >= 0;)
    clips.getUnchecked(i)->removeFromParentTrack();
}

tracktion::WaveAudioClip::Ptr Helper::loadAudioFileAsClip(
    tracktion::Edit &edit, const File &file) {
  // Find the first track and delete all clips from it
  if (auto track = getOrInsertAudioTrackAt(edit, 0)) {
    removeAllClips(*track);

    // Add a new clip to this track
    tracktion::engine::AudioFile audioFile(edit.engine, file);

    if (audioFile.isValid())
      if (auto newClip =
              track->insertWaveClip(file.getFileNameWithoutExtension(), file,
                                    {{0.0, audioFile.getLength()}, 0.0}, false))
        return newClip;
  }

  return {};
}
