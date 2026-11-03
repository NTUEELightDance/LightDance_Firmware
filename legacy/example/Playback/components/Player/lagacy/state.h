#pragma once

#include "player.h"

#define SHOW_TRANSITION 0

class State {
  public:
    virtual ~State() = default;
    virtual void enter(Player& player) = 0;
    virtual void exit(Player& player) = 0;
    virtual void handleEvent(Player& player, Event& event) = 0;
    virtual void update(Player& player) = 0;

  protected:
    State() = default;
};

class ErrorState: public State {
  public:
    static ErrorState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};
class ResetState: public State {
  public:
    static ResetState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};

class ReadyState: public State {
  public:
    static ReadyState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};

class PlayingState: public State {
  public:
    static PlayingState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};

class PauseState: public State {
  public:
    static PauseState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};

class TestState: public State {
  public:
    static TestState& getInstance();
    void enter(Player& player) override;
    void exit(Player& player) override;
    void handleEvent(Player& player, Event& event) override;
    void update(Player& player) override;
};
