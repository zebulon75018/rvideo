#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <cstdint>

/**
 * AudioDecoder
 * ------------
 * Decodes audio from WAV/FLAC/OGG (via libsndfile) and MP3/MP4 (via FFmpeg).
 * Produces interleaved, normalised float32 PCM samples in [-1, 1].
 */
class AudioDecoder : public QObject
{
    Q_OBJECT

public:
    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder() override = default;

    /** Load a file. Returns true on success. */
    bool loadFile(const QString &filePath);

    /** Release all decoded data. */
    void clear();

    // ── Accessors ──────────────────────────────────────────────────────────
    const std::vector<float> &samples()    const { return m_samples;    }
    int                       sampleRate() const { return m_sampleRate; }
    int                       channels()   const { return m_channels;   }
    int64_t                   numFrames()  const { return m_numFrames;  }
    double                    duration()   const;          ///< seconds
    QString                   filePath()   const { return m_filePath;   }
    bool                      isLoaded()   const { return !m_samples.empty(); }

signals:
    void loadProgress(int percent);   ///< 0–100 during decoding
    void loadFinished(bool success, const QString &errorMsg);

private:
    bool loadViaSndFile(const QString &path);
    bool loadViaFFmpeg (const QString &path);

    std::vector<float> m_samples;   ///< interleaved frames
    int     m_sampleRate = 0;
    int     m_channels   = 0;
    int64_t m_numFrames  = 0;
    QString m_filePath;
};
