import argparse
from lerobot.datasets.lerobot_dataset import LeRobotDataset


def main(args):
    dataset = LeRobotDataset(args.dataset_dir)
    dataset.repo_id = args.huggingface_hub
    dataset.push_to_hub()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload LeRobotDataset to HF")
    parser.add_argument(
        "--dataset_dir",
        type=str,
        default=".",
        help="Directory containing the episode folders",
    )
    parser.add_argument("--huggingface_hub", type=str, help="username/hub")

    args = parser.parse_args()

    main(args)
