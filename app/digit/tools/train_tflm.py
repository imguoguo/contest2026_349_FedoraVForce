#!/usr/bin/env python3
############################################################################
# app/digit/tools/train_tflm.py
#
# Train the same 784-128-10 MLP as train.py with TensorFlow, on the same
# data and augmentation, and convert it to a fully int8 .tflite (int8
# input and output too) for TFLite Micro.  Needs tensorflow, e.g. in a
# virtual environment, see README.md.
#
#   train_tflm.py <mnist dir> [-o mnist_mlp.tflite]
############################################################################

import argparse
import os
import sys

import numpy as np
import tensorflow as tf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train import BATCH, EPOCHS, HIDDEN, LR, augment, load_set  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mnist")
    parser.add_argument("-o", "--out", default="mnist_mlp.tflite")
    args = parser.parse_args()

    rng = np.random.default_rng(349)
    tf.keras.utils.set_random_seed(349)
    x_train, y_train = load_set(args.mnist, "train")
    x_test, y_test = load_set(args.mnist, "t10k")

    model = tf.keras.Sequential([
        tf.keras.Input((784,)),
        tf.keras.layers.Dense(HIDDEN, activation="relu"),
        tf.keras.layers.Dense(10),
        tf.keras.layers.Softmax(),
    ])

    schedule = tf.keras.optimizers.schedules.PiecewiseConstantDecay(
        [6 * 469, 12 * 469, 18 * 469], [LR, LR / 2, LR / 4, LR / 8])
    model.compile(optimizer=tf.keras.optimizers.Adam(schedule),
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])

    for epoch in range(EPOCHS):
        x = augment(x_train, rng).reshape(len(x_train), -1)
        model.fit(x, y_train, batch_size=BATCH, epochs=1, verbose=0)
        acc = model.evaluate(x_test.reshape(len(x_test), -1), y_test,
                             verbose=0)[1]
        print("epoch %2d  test %.2f%%" % (epoch + 1, 100 * acc))

    def representative():
        for i in rng.choice(len(x_train), 500, replace=False):
            yield [x_train[i].reshape(1, 784)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite = converter.convert()

    # Accuracy of the int8 model as the board runs it

    interp = tf.lite.Interpreter(model_content=tflite)
    interp.allocate_tensors()
    inp = interp.get_input_details()[0]
    out = interp.get_output_details()[0]
    scale, zero = inp["quantization"]
    correct = 0

    for x, y in zip(x_test.reshape(len(x_test), 1, 784), y_test):
        q = np.clip(np.round(x / scale + zero), -128, 127).astype(np.int8)
        interp.set_tensor(inp["index"], q)
        interp.invoke()
        correct += int(interp.get_tensor(out["index"]).argmax() == y)

    print("int8 tflite test %.2f%%, %d bytes" %
          (100 * correct / len(y_test), len(tflite)))
    print("ops:", sorted({op["op_name"] for op in
                          interp._get_ops_details()}))

    with open(args.out, "wb") as f:
        f.write(tflite)


if __name__ == "__main__":
    main()
