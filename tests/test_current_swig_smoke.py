import numpy as np
import GammaDSP as dsp

def main():
    print("GammaDSP:", dsp.__file__)
    assert hasattr(dsp, "runGenerator")
    assert hasattr(dsp, "runFunction")
    assert hasattr(dsp, "FunctionGraph")
    assert hasattr(dsp, "DSPGraph")

    n = 256

    osc = dsp.Saw(110.0, 0.0)
    x = np.zeros(n, dtype=np.float32)
    dsp.runGenerator(osc, x, n)
    assert x.shape == (n,)
    assert np.isfinite(x).all()

    lp = dsp.LowPassFilter()
    lp.freq(900.0)
    lp.res(0.707)

    y = np.zeros(n, dtype=np.float32)
    dsp.runFunction(lp, x, y, n)
    assert y.shape == (n,)
    assert np.isfinite(y).all()

    graph = dsp.FunctionGraph()
    node = graph.addNode(lp)
    graph.connectInput(node, 1.0)
    graph.setOutput(node, 1.0)

    z = np.zeros(n, dtype=np.float32)
    dsp.runFunction(graph, x, z, n)
    assert z.shape == (n,)
    assert np.isfinite(z).all()

    print("current SWIG smoke OK")

if __name__ == "__main__":
    main()
