
// Persistent RNN Includes
#include <persistent_rnn_high_level.h>

#include <prnn/detail/matrix/matrix.h>
#include <prnn/detail/matrix/random_operations.h>
#include <prnn/detail/matrix/matrix_operations.h>
#include <prnn/detail/matrix/matrix_transforms.h>
#include <prnn/detail/matrix/matrix_view.h>
#include <prnn/detail/matrix/copy_operations.h>
#include <prnn/detail/matrix/operation.h>

#include <prnn/detail/rnn/recurrent_ops_handle.h>
#include <prnn/detail/rnn/recurrent_ops.h>

#include <prnn/detail/parallel/synchronization.h>

#include <prnn/detail/util/logger.h>
#include <prnn/detail/util/timer.h>
#include <prnn/detail/util/argument_parser.h>

// Standard Library Includes
#include <random>
#include <iostream>

static double getFlopCount(prnn::RecurrentOpsHandle& handle)
{
    return 2.0 * handle.layerSize * handle.layerSize * handle.miniBatchSize * handle.timesteps;
}

void benchmarkRnnForward(size_t iterations, size_t layerSize, size_t miniBatchSize,
    size_t timesteps, bool usePersistent, const prnn::matrix::Precision& precision) {

    auto weights     = prnn::matrix::rand({layerSize, layerSize               }, precision);
    auto activations = prnn::matrix::rand({layerSize, miniBatchSize, timesteps}, precision);

    prnn::RecurrentOpsHandle handle(layerSize, miniBatchSize, timesteps,
        prnn::RecurrentRectifiedLinear(),
        prnn::RECURRENT_FORWARD,
        usePersistent);

    auto scratch = prnn::rnn::getForwardPropScratch(handle, precision);

    // warm up
    prnn::rnn::forwardPropRecurrent(prnn::matrix::DynamicView(activations),
        prnn::matrix::ConstDynamicView(weights),
        prnn::matrix::DynamicView(scratch), handle);
    prnn::parallel::synchronize();

    prnn::util::Timer timer;

    timer.start();

    for (size_t i = 0; i < iterations; ++i) {
        prnn::rnn::forwardPropRecurrent(prnn::matrix::DynamicView(activations),
            prnn::matrix::ConstDynamicView(weights),
            prnn::matrix::DynamicView(scratch), handle);
    }

    prnn::parallel::synchronize();

    timer.stop();

    double totalFlops = iterations * getFlopCount(handle);

    double teraflops = totalFlops / (timer.seconds() * 1.0e12);
    double microsecondsPerKernel = timer.seconds() * 1.0e6 / iterations;

    std::cout << "RNN Forward Propagation: " << teraflops << " TFLOPS/s\n";
    std::cout << "RNN Average Kernel Time: " << microsecondsPerKernel << " us\n";
}

void benchmarkRnnReverse(size_t iterations, size_t layerSize, size_t miniBatchSize,
    size_t timesteps, bool usePersistent, const prnn::matrix::Precision& precision)
{
    auto weights     = prnn::matrix::rand({layerSize, layerSize               }, precision);
    auto activations = prnn::matrix::rand({layerSize, miniBatchSize, timesteps}, precision);
    auto deltas      = prnn::matrix::rand({layerSize, miniBatchSize, timesteps}, precision);

    prnn::RecurrentOpsHandle handle(layerSize, miniBatchSize, timesteps,
        prnn::RecurrentRectifiedLinear(),
        prnn::RECURRENT_FORWARD,
        usePersistent);

    auto scratch = prnn::rnn::getBackPropDeltasScratch(handle, precision);

    // warm up
    prnn::rnn::backPropDeltasRecurrent(prnn::matrix::DynamicView(deltas),
        prnn::matrix::ConstDynamicView(weights),
        prnn::matrix::DynamicView(activations),
        prnn::matrix::DynamicView(scratch), handle);
    prnn::parallel::synchronize();

    prnn::util::Timer timer;

    timer.start();

    for (size_t i = 0; i < iterations; ++i) {
        prnn::rnn::backPropDeltasRecurrent(prnn::matrix::DynamicView(deltas),
            prnn::matrix::ConstDynamicView(weights),
            prnn::matrix::DynamicView(activations),
            prnn::matrix::DynamicView(scratch), handle);
    }

    prnn::parallel::synchronize();

    timer.stop();

    double totalFlops = iterations * getFlopCount(handle);

    double teraflops = totalFlops / (timer.seconds() * 1.0e12);
    double microsecondsPerKernel = timer.seconds() * 1.0e6 / iterations;

    std::cout << "RNN Back Propagation Deltas: " << teraflops << " TFLOPS/s\n";
    std::cout << "RNN Average Kernel Time: " << microsecondsPerKernel << " us\n";
}

void benchmarkRnnGradients(size_t iterations, size_t layerSize, size_t miniBatchSize,
    size_t timesteps, bool usePersistent, const prnn::matrix::Precision& precision)
{
    auto weights     = prnn::matrix::rand({layerSize, layerSize               }, precision);
    auto activations = prnn::matrix::rand({layerSize, miniBatchSize, timesteps}, precision);
    auto deltas      = prnn::matrix::rand({layerSize, miniBatchSize, timesteps}, precision);

    prnn::RecurrentOpsHandle handle(layerSize, miniBatchSize, timesteps,
        prnn::RecurrentRectifiedLinear(),
        prnn::RECURRENT_FORWARD,
        usePersistent);

    auto scratch = prnn::rnn::getBackPropGradientsScratch(handle, precision);

    // warm up
    prnn::rnn::backPropGradientsRecurrent(
        prnn::matrix::DynamicView(weights),
        prnn::matrix::ConstDynamicView(activations),
        prnn::matrix::ConstDynamicView(deltas),
        prnn::matrix::DynamicView(scratch), handle);
    prnn::parallel::synchronize();

    prnn::util::Timer timer;

    timer.start();

    for (size_t i = 0; i < iterations; ++i) {
        prnn::rnn::backPropGradientsRecurrent(
            prnn::matrix::DynamicView(weights),
            prnn::matrix::ConstDynamicView(activations),
            prnn::matrix::ConstDynamicView(deltas),
            prnn::matrix::DynamicView(scratch), handle);
    }

    prnn::parallel::synchronize();

    timer.stop();

    double totalFlops = iterations * getFlopCount(handle);

    double teraflops = totalFlops / (timer.seconds() * 1.0e12);
    double microsecondsPerKernel = timer.seconds() * 1.0e6 / iterations;

    std::cout << "RNN Back Propagation Deltas: " << teraflops << " TFLOPS/s\n";
    std::cout << "RNN Average Kernel Time: " << microsecondsPerKernel << " us\n";

}

void runBenchmark(size_t iterations, size_t layerSize, size_t miniBatchSize,
    size_t timesteps, bool usePersistent, prnn::matrix::Precision& precision)
{
    benchmarkRnnForward(  iterations, layerSize, miniBatchSize, timesteps, usePersistent, precision);
    benchmarkRnnReverse(  iterations, layerSize, miniBatchSize, timesteps, usePersistent, precision);
    benchmarkRnnGradients(iterations, layerSize, miniBatchSize, timesteps, usePersistent, precision);
}

int main(int argc, char** argv) {

    prnn::util::ArgumentParser parser(argc, argv);

    prnn::matrix::Precision precision = prnn::matrix::SinglePrecision();

    size_t iterations     = 20;
    size_t layerSize      = prnn::rnn::getMaximumSizeRNNForThisGPU(precision);
    size_t miniBatcheSize = 2;
    size_t timesteps      = 64;
    bool   usePersistent  = true;

    parser.parse("-i", "--iterations",      iterations,     iterations,     "Iterations to run each recurrent operation.");
    parser.parse("-l", "--layer-size",      layerSize,      layerSize,      "The size of the recurrent layer.");
    parser.parse("-b", "--mini-batch-size", miniBatcheSize, miniBatcheSize, "The number of utterances per mini-batch.");
    parser.parse("-t", "--timesteps",       timesteps,      timesteps,      "The length of each utterance.");
    parser.parse("-p", "--no-peristent",    usePersistent,  usePersistent,  "Disable use of persistent kernels.");

    parser.parse();

    runBenchmark(iterations, layerSize, miniBatcheSize, timesteps, usePersistent, precision);
}

