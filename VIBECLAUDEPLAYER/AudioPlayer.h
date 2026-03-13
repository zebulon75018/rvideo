#pragma once

#include <QObject>
#include <QTimer>
#include <vector>
#include <atomic>
#include <cstdint>

#include <portaudio.h>

/**
 * AudioPlayer
 * -----------
 * Plays interleaved float32 PCM samples via PortAudio.
 * All position/state queries are thread-safe (atomic).
 * Signals are emitted from the GUI thread via a QTimer poll.
 */
class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Playing, Paused };

    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    /**
     * Provide the sample buffer to play back.
     * The caller must keep the vector alive as long as AudioPlayer exists.
     */
    void setSamples(const std::vector<float> *samples,
                    int sampleRate,
                    int channels);

    // ── Transport ───────────────────────────────────────────────────────────
    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);

    // ── Queries ─────────────────────────────────────────────────────────────
    double  currentPositionSeconds() const;
    double  durationSeconds()        const;
    State   state()                  const { return m_state.load(); }
    bool    isPlaying()              const { return m_state == State::Playing; }
    bool    isPaused()               const { return m_state == State::Paused;  }

signals:
    void positionChanged(double seconds);
    void stateChanged(AudioPlayer::State newState);
    void playbackFinished();

private:
    // PortAudio callback (static trampoline)
    static int paCallbackStatic(const void *in, void *out,
                                unsigned long frames,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags flags,
                                void *userData);

    int paCallback(void *out, unsigned long frames);

    void openStream();
    void closeStream();
    void notifyState(State s);

    // ── PA state ────────────────────────────────────────────────────────────
    PaStream *m_stream   = nullptr;
    bool      m_paInited = false;

    // ── Sample data ─────────────────────────────────────────────────────────
    const std::vector<float> *m_samples    = nullptr;
    int                       m_sampleRate = 44100;
    int                       m_channels   = 2;

    // ── Playback position (written by PA thread, read by GUI thread) ─────────
    std::atomic<int64_t>  m_currentFrame{0};
    std::atomic<State>    m_state{State::Stopped};

    // ── GUI-thread timer for emitting positionChanged ─────────────────────-
    QTimer *m_pollTimer = nullptr;
    double  m_lastEmittedPos = -1.0;
};
