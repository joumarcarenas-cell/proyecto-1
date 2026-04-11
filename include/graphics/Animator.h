#pragma once
#include <raylib.h>

namespace Graphics {
class Animator {
public:
  Animator(Texture2D spriteSheet, int frameWidth, int frameHeight)
      : m_spriteSheet(spriteSheet), m_frameW(frameWidth), m_frameH(frameHeight),
        m_currentRow(0), m_currentCol(0), m_startCol(0), m_endCol(0),
        m_frameDuration(0.1f), m_timer(0.0f), m_isLooping(true),
        m_isFinished(false) {}

  void Update(float deltaTime) {
    if (m_isFinished)
      return;
    m_timer += deltaTime;
    if (m_timer >= m_frameDuration) {
      m_timer = 0.0f;
      m_currentCol++;
      if (m_currentCol > m_endCol) {
        if (m_isLooping) {
          m_currentCol = m_startCol;
        } else {
          m_currentCol = m_endCol;
          m_isFinished = true;
        }
      }
    }
  }

  void Play(int row, int startCol, int endCol, float frameDuration,
            bool loop = true) {
    if (m_currentRow == row && m_startCol == startCol && m_endCol == endCol &&
        m_isLooping == loop) {
      return; // Already playing this animation
    }
    m_currentRow = row;
    m_startCol = startCol;
    m_endCol = endCol;
    m_currentCol = startCol;
    m_frameDuration = frameDuration;
    m_isLooping = loop;
    m_isFinished = false;
    m_timer = 0.0f;
  }

  Rectangle GetCurrentFrameRec() const {
    return {(float)m_currentCol * m_frameW, (float)m_currentRow * m_frameH,
            (float)m_frameW, (float)m_frameH};
  }

  bool IsFinished() const { return m_isFinished; }

private:
  Texture2D m_spriteSheet;
  int m_frameW, m_frameH;
  int m_currentRow, m_currentCol, m_startCol, m_endCol;

  float m_frameDuration;
  float m_timer;
  bool m_isLooping;
  bool m_isFinished;
};
} // namespace Graphics
