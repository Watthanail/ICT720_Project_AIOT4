import seoulai_gym as gym
from seoulai_gym.envs.checkers.agents import RandomAgentLight
from seoulai_gym.envs.checkers.agents import RandomAgentDark
import time


def main():
    env = gym.make("Checkers")

    a1 = RandomAgentLight()
    a2 = RandomAgentDark()

    obs = env.reset()
    env.render()  # Render the initial state

    current_agent = a1
    next_agent = a2

    while True:
        from_row, from_col, to_row, to_col = current_agent.act(obs)
        obs, rew, done, info = env.step(current_agent, from_row, from_col, to_row, to_col)
        current_agent.consume(obs, rew, done)

        env.render()  # Render the updated state after each step

        if done:
            winner = "Light" if current_agent == a1 else "Dark"
            print(f"Game over! {winner} agent wins.")
            obs = env.reset()
            env.render()  # Render the final state
            break  # Exit the loop when the game is over

        # Introduce a delay of 0.5 seconds between each step
        time.sleep(0.5)

        # switch agents
        temporary_agent = current_agent
        current_agent = next_agent
        next_agent = temporary_agent

    env.close()


if __name__ == "__main__":
    main()
