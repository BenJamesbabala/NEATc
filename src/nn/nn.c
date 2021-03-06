#include <nn.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

static inline float nn_sigmoid(float input)
{
	if(input < -45.0){
		return 0;
	}else if(input > 45.0){
		return 1;
	}

	return 1.0 / (1 + exp(-input));
}

static inline float nn_fast_sigmoid(float input)
{
	return input / (1 + fabs(input)); 
}

static inline float nn_relu(float input)
{
	if(input < 0){
		return 0.0;
	}

	return input;
}

static inline float nn_rand(float start, float end)
{
	assert(start < end);

	float range = end - start;

	return (float)rand() / (float)(RAND_MAX / range) + start;
}

static inline float nn_activate(enum nn_activation activation, float input)
{
	switch(activation){
		case NN_ACTIVATION_SIGMOID:
			return nn_sigmoid(input);
		case NN_ACTIVATION_FAST_SIGMOID:
			return nn_fast_sigmoid(input);
		case NN_ACTIVATION_RELU:
			return nn_relu(input);
		default:
			fprintf(stderr,
				"Activation function \"%d\" not found\n",
				activation);
			exit(-1);
	}
}

static void nn_ffnet_set_pointers(struct nn_ffnet *net)
{
	assert(net);

	net->weight = (float*)((char*)net + sizeof(struct nn_ffnet));
	net->output = net->weight + net->nweights;
}


struct nn_ffnet *nn_ffnet_create(size_t input_count,
				 size_t hidden_count,
				 size_t output_count,
				 size_t hidden_layer_count)
{
	assert(input_count > 0);
	assert(output_count > 0);
	assert((hidden_count > 0) == (hidden_layer_count > 0));

	size_t hidden_weights = 0;
	if(hidden_layer_count > 0){
		size_t input_weights = (input_count + 1) * hidden_count;
		size_t hidden_internal_weights = (hidden_layer_count - 1) *
						 (hidden_count + 1) *
						 hidden_count;
		hidden_weights = input_weights + hidden_internal_weights;
	}

	size_t output_weights;
	if(hidden_layer_count > 0){
		output_weights = hidden_count + 1;
	}else{
		output_weights = input_count + 1;
	}
	output_weights *= output_count;

	size_t total_weights = hidden_weights + output_weights;
	size_t total_neurons = input_count +
			       hidden_count * hidden_layer_count +
			       output_count;

	size_t total_items = total_weights + total_neurons;

	/* Allocate the struct with extra bytes behind it for the data */
	size_t items_bytes = sizeof(float) * total_items;
	struct nn_ffnet *net = malloc(sizeof(struct nn_ffnet) + items_bytes);
	assert(net);

	/* Set the extra data to 0 */
	memset(net + 1, 0, items_bytes);

	net->ninputs = input_count;
	net->nhiddens = hidden_count;
	net->noutputs = output_count;
	net->nhidden_layers = hidden_layer_count;

	net->nweights = total_weights;
	net->nneurons = total_neurons;

	/* Default values */
	net->hidden_activation = NN_ACTIVATION_SIGMOID;
	net->output_activation = NN_ACTIVATION_SIGMOID;

	net->bias = -1.0;

	nn_ffnet_set_pointers(net);

	return net;
}

struct nn_ffnet *nn_ffnet_copy(struct nn_ffnet *net)
{
	assert(net);

	size_t extra = net->nweights + net->nneurons;
	size_t bytes = sizeof(struct nn_ffnet) + sizeof(float) * extra;
	assert(bytes > sizeof(struct nn_ffnet));

	struct nn_ffnet *new = malloc(bytes);
	assert(new);

	memcpy(new, net, bytes);

	nn_ffnet_set_pointers(new);

	return new;
}

void nn_ffnet_destroy(struct nn_ffnet *net)
{
	assert(net);

	free(net);
}

void nn_ffnet_randomize(struct nn_ffnet *net)
{
	assert(net);

	for(int i = 0; i < net->nweights; i++){
		net->weight[i] = nn_rand(-0.5, 0.5);
	}
}

void nn_ffnet_set_activations(struct nn_ffnet *net,
			      enum nn_activation hidden,
			      enum nn_activation output)
{
	assert(net);

	net->hidden_activation = hidden;
	net->output_activation = output;
}

void nn_ffnet_set_bias(struct nn_ffnet *net, float bias)
{
	assert(net);

	net->bias = bias;
}

float *nn_ffnet_run(struct nn_ffnet *net, const float *inputs)
{
	assert(net);

	/* Copy the inputs to the extra output memory space so we don't have
	 * to make a special case for the input layer, it will look like this:
	 * [ **struct**, weight.. , input.., output.., **delta** ]
	 */
	float *input = net->output;
	memcpy(input, inputs, sizeof(float) * net->ninputs);

	/* Calculate hidden layers */
	float *weight = net->weight;
	float *output = net->output + net->ninputs;
	for(size_t i = 0; i < net->nhidden_layers; i++){
		/* First get all the inputs, then get all the hidden layers */
		size_t nweights = net->nhiddens;
		if(i == 0){
			nweights = net->ninputs;
		}

		for(size_t j = 0; j < net->nhiddens; j++){
			/* Start with the bias */
			float sum = *weight++ * net->bias;

			/* Sum the rest of the weights */
			for(size_t k = 0; k < nweights; k++){
				sum += *weight++ * input[k];
			}

			*output++ = nn_activate(net->hidden_activation, sum);
		}

		input += nweights;
	}

	/* The return value must be saved because the output pointer is going
	 * to be changed by the output layer calculation */
	float *ret = output;

	size_t nweights = net->nhiddens;
	/* Get the input layer if there are no hidden layers */
	if(net->nhidden_layers == 0){
		nweights = net->ninputs;
	}

	/* Calculate output layer */
	for(size_t i = 0; i < net->noutputs; i++){
		/* Start with the bias */
		float sum = *weight++ * net->bias;

		/* Sum the rest of the weights */
		for(size_t j = 0; j < nweights; j++){
			sum += *weight++ * input[j];
		}

		*output++ = nn_activate(net->output_activation, sum);
	}

	assert(weight - net->weight == net->nweights);
	assert(output - net->output == net->nneurons);

	return ret;
}
