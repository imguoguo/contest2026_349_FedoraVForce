#!/usr/bin/env python3
############################################################################
# app/digit/tools/train.py
#
# Train the 784-128-10 MLP used by paint for digit recognition.
# Only numpy is needed.  MNIST is not part of the repository, see README.md
# for where to download the four *-ubyte.gz files.
#
#   train.py <mnist dir> [-o mnist_mlp.npz]
#
# The output keeps int8 weights with one float scale per layer, which is
# exactly what gen_model.py writes into C.
############################################################################

import argparse
import gzip
import os

import numpy as np

EPOCHS = 20
BATCH = 128
HIDDEN = 128
LR = 1e-3


def load(path, offset):
    with gzip.open(path, "rb") as f:
        return np.frombuffer(f.read(), np.uint8, offset=offset)


def load_set(root, prefix):
    x = load(os.path.join(root, prefix + "-images-idx3-ubyte.gz"), 16)
    y = load(os.path.join(root, prefix + "-labels-idx1-ubyte.gz"), 8)
    return x.reshape(-1, 28, 28).astype(np.float32) / 255, y.astype(np.int64)


def shift(img, dy, dx):
    """Move a batch of images by (dy, dx) pixels, filling with zeros"""
    out = np.zeros_like(img)
    h, w = img.shape[1:]
    out[:, max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = \
        img[:, max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return out


def augment(x, rng):
    """Random +-2px translation, and thicker strokes for half the batch"""
    out = np.empty_like(x)
    for i in range(len(x)):
        out[i] = shift(x[i:i + 1], *rng.integers(-2, 3, 2))[0]

    bold = rng.random(len(x)) < 0.5
    thick = np.maximum(out[bold], shift(out[bold], 0, 1))
    out[bold] = np.maximum(thick, shift(thick, 1, 0))
    return out


def quantize(w):
    scale = np.abs(w).max() / 127
    return np.round(w / scale).astype(np.int8), np.float32(scale)


def forward(x, w1, b1, w2, b2):
    h = np.maximum(x @ w1.T + b1, 0)
    return h, h @ w2.T + b2


def accuracy(x, y, w1, b1, w2, b2):
    return (forward(x.reshape(len(x), -1), w1, b1, w2, b2)[1].argmax(1) ==
            y).mean()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mnist")
    parser.add_argument("-o", "--out", default="mnist_mlp.npz")
    args = parser.parse_args()

    rng = np.random.default_rng(349)
    x_train, y_train = load_set(args.mnist, "train")
    x_test, y_test = load_set(args.mnist, "t10k")

    params = [
        rng.normal(0, np.sqrt(2 / 784), (HIDDEN, 784)).astype(np.float32),
        np.zeros(HIDDEN, np.float32),
        rng.normal(0, np.sqrt(2 / HIDDEN), (10, HIDDEN)).astype(np.float32),
        np.zeros(10, np.float32),
    ]

    # Adam

    m = [np.zeros_like(p) for p in params]
    v = [np.zeros_like(p) for p in params]
    step = 0

    for epoch in range(EPOCHS):
        order = rng.permutation(len(x_train))
        lr = LR * (0.5 ** (epoch // 6))

        for i in range(0, len(order), BATCH):
            idx = order[i:i + BATCH]
            x = augment(x_train[idx], rng).reshape(len(idx), -1)
            y = y_train[idx]
            w1, b1, w2, b2 = params

            h, logits = forward(x, w1, b1, w2, b2)
            p = np.exp(logits - logits.max(1, keepdims=True))
            p /= p.sum(1, keepdims=True)
            p[np.arange(len(y)), y] -= 1
            p /= len(y)

            dh = (p @ w2) * (h > 0)
            grads = [dh.T @ x, dh.sum(0), p.T @ h, p.sum(0)]

            step += 1
            for j, g in enumerate(grads):
                m[j] = 0.9 * m[j] + 0.1 * g
                v[j] = 0.999 * v[j] + 0.001 * g * g
                params[j] -= lr * (m[j] / (1 - 0.9 ** step)) / \
                             (np.sqrt(v[j] / (1 - 0.999 ** step)) + 1e-8)

        print("epoch %2d  test %.2f%%" %
              (epoch + 1, 100 * accuracy(x_test, y_test, *params)))

    w1, b1, w2, b2 = params
    q1, s1 = quantize(w1)
    q2, s2 = quantize(w2)
    acc = accuracy(x_test, y_test, q1 * s1, b1, q2 * s2, b2)
    print("int8 test %.2f%%" % (100 * acc))

    np.savez(args.out, w1=q1, s1=s1, b1=b1, w2=q2, s2=s2, b2=b2,
             accuracy=np.float32(acc))


if __name__ == "__main__":
    main()
