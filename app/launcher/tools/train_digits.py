# Train the 784-128-10 digit MLP on MNIST with numpy and write model.json.
# Data: the four MNIST .gz files from https://storage.googleapis.com/cvdf-datasets/mnist/ in the current directory.
# Usage: python3 train_digits.py && python3 gen_model.py model.json ../launcher_model.c
import gzip, numpy as np, json, base64, time
def imgs(f): return np.frombuffer(gzip.open(f).read(), np.uint8, offset=16).reshape(-1, 28, 28).astype(np.float32) / 255
def lbls(f): return np.frombuffer(gzip.open(f).read(), np.uint8, offset=8).astype(np.int64)
Xtr, ytr = imgs('train-images-idx3-ubyte.gz'), lbls('train-labels-idx1-ubyte.gz')
Xte, yte = imgs('t10k-images-idx3-ubyte.gz'), lbls('t10k-labels-idx1-ubyte.gz')
rng = np.random.default_rng(0)

def augment(b):
    out = np.zeros_like(b)
    for i, im in enumerate(b):
        dx, dy = rng.integers(-2, 3, 2)
        im = np.roll(np.roll(im, dy, 0), dx, 1)
        # random stroke thickening: max with shifted copy
        if rng.random() < 0.5:
            im = np.maximum(im, np.roll(im, 1, rng.integers(0, 2)))
        out[i] = im
    return out

H = 128
W1 = rng.normal(0, np.sqrt(2 / 784), (784, H)).astype(np.float32); b1 = np.zeros(H, np.float32)
W2 = rng.normal(0, np.sqrt(2 / H), (H, 10)).astype(np.float32); b2 = np.zeros(10, np.float32)
params = [W1, b1, W2, b2]; vel = [np.zeros_like(p) for p in params]

def forward(X):
    h = np.maximum(X @ W1 + b1, 0)
    z = h @ W2 + b2
    return h, z

lr, mom, bs = 0.05, 0.9, 128
t0 = time.time()
for ep in range(10):
    idx = rng.permutation(len(Xtr))
    for s in range(0, len(idx), bs):
        j = idx[s:s + bs]
        X = augment(Xtr[j]).reshape(len(j), -1); y = ytr[j]
        h, z = forward(X)
        z -= z.max(1, keepdims=True); p = np.exp(z); p /= p.sum(1, keepdims=True)
        p[np.arange(len(j)), y] -= 1; p /= len(j)
        gW2 = h.T @ p; gb2 = p.sum(0)
        dh = (p @ W2.T) * (h > 0)
        gW1 = X.T @ dh; gb1 = dh.sum(0)
        for k, g in enumerate([gW1, gb1, gW2, gb2]):
            vel[k] = mom * vel[k] - lr * (g + (1e-4 * params[k] if k % 2 == 0 else 0))
            params[k] += vel[k]
    lr *= 0.7
    acc = (forward(Xte.reshape(len(Xte), -1))[1].argmax(1) == yte).mean()
    print(f'epoch {ep} test acc {acc:.4f} ({time.time() - t0:.0f}s)', flush=True)

# int8 symmetric per-tensor quantization of weights; biases stay float
def q(w):
    s = np.abs(w).max() / 127
    return np.clip(np.round(w / s), -127, 127).astype(np.int8), float(s)
q1, s1 = q(W1); q2, s2 = q(W2)
Wq1 = q1.astype(np.float32) * s1; Wq2 = q2.astype(np.float32) * s2
hq = np.maximum(Xte.reshape(len(Xte), -1) @ Wq1 + b1, 0)
accq = ((hq @ Wq2 + b2).argmax(1) == yte).mean()
print(f'int8 weights test acc {accq:.4f}')
json.dump({'h': H, 's1': s1, 's2': s2,
           'w1': base64.b64encode(q1.tobytes()).decode(), 'w2': base64.b64encode(q2.tobytes()).decode(),
           'b1': [round(float(x), 5) for x in b1], 'b2': [round(float(x), 5) for x in b2],
           'acc_float': float(acc), 'acc_int8': float(accq)}, open('model.json', 'w'))
np.savez('model_float.npz', W1=W1, b1=b1, W2=W2, b2=b2)
