import gym
import random

env = gym.make("CartPole-v1", render_mode="human")

episodes = 20
for episode in range(1, episodes+1):
    state = env.reset()
    done = False
    score = 0

    while not done:
        action = random.choice([0, 1])
        step_result = env.step(action)
        state, reward, done, _ = step_result[:4]
        score += reward
        env.render()
    
    print(f"Episode {episode}, Score: {score}")

env.close()
