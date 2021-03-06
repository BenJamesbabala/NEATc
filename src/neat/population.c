#include "population.h"

#include <float.h>
#include <assert.h>

static void neat_reset_genomes(struct neat_pop *p)
{
	assert(p);

	/* Create a base genome and copy it for every other one */
	p->genomes[0] = neat_genome_create(p->conf, p->innovation++);

	for(size_t i = 1; i < p->ngenomes; i++){
		p->genomes[i] = neat_genome_copy(p->genomes[0]);
	}
}

static void neat_replace_genome(struct neat_pop *p,
				size_t dest,
				struct neat_genome *src)
{
	assert(p);
	assert(src);
	assert(p->genomes[dest] != src);

	neat_genome_destroy(p->genomes[dest]);
	p->genomes[dest] = neat_genome_copy(src);
}

static struct neat_species *neat_create_new_species(struct neat_pop *p,
						    struct neat_genome *base)
{
	assert(p);

	p->species = realloc(p->species,
			     sizeof(struct neat_species*) * ++p->nspecies);
	assert(p->species);

	struct neat_species *new = neat_species_create(p->conf, base);
	p->species[p->nspecies - 1] = new;

	return new;
}

static bool neat_find_worst_fitness(struct neat_pop *p, size_t *worst_genome)
{
	assert(p);
	assert(worst_genome);

	bool found_worst = false;

	float worst_fitness = FLT_MAX;
	for(size_t i = 0; i < p->ngenomes; i++){
		struct neat_genome *genome = p->genomes[i];

		float fitness = genome->fitness;
		if(fitness < worst_fitness &&
		   genome->time_alive > p->conf.genome_minimum_ticks_alive){
			*worst_genome = i;
			worst_fitness = fitness;
			found_worst = true;
		}
	}

	return found_worst;
}

static float neat_get_species_fitness_average(struct neat_pop *p)
{
	assert(p);

	float total_avg = 0.0;
	for(size_t i = 0; i < p->nspecies; i++){
		total_avg += neat_species_get_average_fitness(p->species[i]);
	}
	total_avg /= (double)p->nspecies;

	return total_avg;
}

static void neat_speciate_genome(struct neat_pop *p, size_t genome_id)
{
	assert(p);

	struct neat_genome *genome = p->genomes[genome_id];
	float compatibility_treshold = p->conf.genome_compatibility_treshold;

	/* Add genome to species if the representant matches the genome */
	for(size_t i = 0; i < p->nspecies; i++){
		struct neat_genome *species_representant =
			neat_species_get_representant(p->species[i]);
		if(neat_genome_is_compatible(genome,
					     species_representant,
					     compatibility_treshold)){
			neat_species_add_genome(p->species[i], genome);
			return;
		}
	}

	/* If no matching species could be found create a new species */
	struct neat_species *new = neat_create_new_species(p, NULL);
	neat_species_add_genome(new, genome);
}

static void neat_select_reproduction_species(struct neat_pop *p,
					     size_t worst_genome)
{
	assert(p);

	float total_avg = neat_get_species_fitness_average(p);

	float selection_random = (float)rand() / (float)RAND_MAX;
	for(size_t i = 0; i < p->nspecies; i++){
		struct neat_species *s = p->species[i];

		/* Ignore empty species */
		if(s->ngenomes == 0){
			continue;
		}

		float avg = neat_species_get_average_fitness(s);
		float selection_prob = avg / total_avg;

		/* If we didn't find a match, 
		 * reduce the chance to find a new one
		 */
		if(selection_random > selection_prob){
			selection_random -= selection_prob;
			continue;
		}

		float random = (float)rand() / (float)RAND_MAX;
		if(random < p->conf.species_crossover_probability){
			/* Do a crossover */
			//TODO: do crossover
		}else{
			/* Select a random genome from the species */
			struct neat_genome *g = neat_species_select_genitor(s);
			neat_replace_genome(p, worst_genome, g);
		}

		neat_speciate_genome(p, worst_genome);

		break;
	}
}

neat_t neat_create(struct neat_config config)
{
	assert(config.population_size > 0);

	struct neat_pop *p = calloc(1, sizeof(struct neat_pop));
	assert(p);

	p->solved = false;
	p->conf = config;
	p->innovation = 1;

	/* Create a genome and copy it n times where n is the population size */
	p->ngenomes = config.population_size;
	p->genomes = malloc(sizeof(struct neat_genome*) *
			    config.population_size);
	assert(p->ngenomes);

	neat_reset_genomes(p);

	/* Create the starting species */
	p->nspecies = 0;
	p->species = NULL;
	neat_create_new_species(p, p->genomes[0]);

	return p;
}

void neat_destroy(neat_t population)
{
	struct neat_pop *p = population;
	assert(p);

	for(size_t i = 0; i < p->ngenomes; i++){
		neat_genome_destroy(p->genomes[i]);
	}
	free(p->genomes);

	for(size_t i = 0; i < p->nspecies; i++){
		neat_species_destroy(p->species[i]);
	}
	free(p->species);
	free(p);
}

const float *neat_run(neat_t population,
		      size_t genome_id,
		      const float *inputs)
{
	struct neat_pop *p = population;
	assert(p);
	assert(genome_id < p->ngenomes);

	return neat_genome_run(p->genomes[genome_id], inputs);
}

void neat_epoch(neat_t population)
{
	struct neat_pop *p = population;
	assert(p);

	size_t worst_genome = 0;
	if(!neat_find_worst_fitness(p, &worst_genome)){
		return;
	}

	/* Remove the worst genome from the species if it contains it */
	for(size_t i = 0; i < p->nspecies; i++){
		neat_species_remove_genome(p->species[i],
					   p->genomes[worst_genome]);
	}

	neat_select_reproduction_species(p, worst_genome);
}

void neat_set_fitness(neat_t population, size_t genome_id, float fitness)
{
	struct neat_pop *p = population;
	assert(p);
	assert(genome_id < p->ngenomes);

	p->genomes[genome_id]->fitness = fitness;
}

void neat_increase_time_alive(neat_t population, size_t genome_id)
{
	struct neat_pop *p = population;
	assert(p);
	assert(genome_id < p->ngenomes);

	p->genomes[genome_id]->time_alive++;
}
