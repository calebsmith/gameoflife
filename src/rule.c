#include "rule.h"

/*
 * Given a rule and a file stream, read and parse the rule file, applying the
 * values to the rule structure. Must call rule_destroy with this rule when
 * no longer in use.
 *
 * Parameters: Rule_t* rule, FILE* file_stream
 * Returns: 0 for success, otherwise returns an exit code for termination
 * Side-Effects: Assign values to rule, allocating memory as needed.
 */
int rule_init(Rule_t* rule, FILE* infile) {
    int num_states;
    int state;
    unsigned char state_char;
    char state_color[COLOR_STRING_LENGTH];
    char neighbor_string[NEIGHBOR_STRING_LENGTH];
    char scm_string[140];
    char scm_filename_string[140];
    char scm_module_string[140];
    char scm_function_string[140];
    int scm_number = 1;
    int color_digit;
    int num_transitions;
    int i, j;

    // Lookup table for neighbor counting functions
    NeighborFunction_t NEIGHBOR_FUNC_LOOKUP[] = {
        [NEIGHBOR_MOORE]=&board_count_moore_neighbors,
        [NEIGHBOR_VON_NEUMANN]=&board_count_von_neumann_neighbors
    };
    // Read number of states
    if (fscanf(infile, "%d", &num_states) == 1) {
        if (num_states <= 0 || num_states > MAX_STATE) {
            printf(
                "Number of states in rule (%d) file is out of bounds\n",
                num_states
            );
            return 1;
        }
        rule->num_states = (unsigned short int) num_states;
    } else {
        printf("Number of states not found in rule file\n");
        return 1;
    }
    // Read state display data. (Character and color values for each state)
    for (i = 0; i < num_states; i++) {
        if (fscanf(infile, "%d:%c:%6s", &state, &state_char, state_color) == 3) {
            if (state < 0 || state > num_states) {
                printf(
                    "State number %d is out of bounds in rule file\n", state
                );
                return 1;
            }
            rule->state_chars[state] = (unsigned short int) state_char;
            // Interpret color string and set value
            rule->state_colors[state].red = (unsigned char) 0;
            rule->state_colors[state].blue = (unsigned char) 0;
            rule->state_colors[state].green = (unsigned char) 0;
            for (j = 0; j < COLOR_STRING_LENGTH; j++) {
                color_digit = (int) state_color[j];
                if (isdigit(color_digit)) {
                    color_digit -= '0';
                } else if (color_digit <= 'F' && color_digit >= 'A') {
                    color_digit = 10 + color_digit - 'A';
                } else if (color_digit <= 'f' && color_digit >= 'a') {
                    color_digit = 10 + color_digit - 'a';
                } else {
                    printf("Illegal character for color in rule file: %s\n", state_color);
                    return 1;
                }
                if (j % 2 == 0) {
                    color_digit *= 16;
                }
                if (j < 2) {
                    rule->state_colors[state].red += color_digit;
                } else if (j >= 2 && j <= 3) {
                    rule->state_colors[state].green += color_digit;
                } else {
                    rule->state_colors[state].blue += color_digit;
                }
            }
        } else {
            printf("Bad syntax in rule file\n");
            printf("Example of format: 1: :FEFEFE\n");
            return 1;
        }
    }
    // Read scheme file:module:function definition
    if (fscanf(infile, "%140s", scm_string) == 1) {
        if (sscanf(scm_string, "%140[^:]:%140[^:]:%140s",
            scm_filename_string, scm_module_string, scm_function_string) == 3) {
            rule->scm = true;
            scm_c_primitive_load(scm_filename_string);
            rule->scm_cell_func = scm_variable_ref(scm_c_public_lookup(
                scm_module_string, scm_function_string));
            rule->transition_length = 0;
            return 0;
        } else if (sscanf(scm_string, "%d", &scm_number) == 1) {
            if (scm_number == 0) {
                rule->scm = false;
            } else {
                printf("Scheme filename/module definition is not valid\n");
                return 1;
            }
        } else {
            printf("Scheme filename/module definition is not valid\n");
            return 1;
        }
    } else {
        printf("Scheme filename/module defition invalid or not found in rule file\n");
        return 1;
    }
    // Read neighbor type definition
    if (fscanf(infile, "%20s", neighbor_string) == 1) {
        if (strncmp(neighbor_string, "moore", 6) == 0) {
            rule->neighbor_func = NEIGHBOR_FUNC_LOOKUP[NEIGHBOR_MOORE];
        } else if (strncmp(neighbor_string, "von_neumann", 11) == 0) {
            rule->neighbor_func = NEIGHBOR_FUNC_LOOKUP[NEIGHBOR_VON_NEUMANN];
        } else {
            printf("Neighbor type is not valid\n");
            return 1;
        }
    } else {
        printf("Neighbor type definition invalid or not found in rule file\n");
        return 1;
    }
    // Read in data for transitions
    if (fscanf(infile, "%d", &num_transitions) == 1) {
        if (num_transitions < 0 || num_transitions > MAX_STATE * MAX_STATE * 2) {
            printf("Number of transitions %d is out of bounds in rule file\n",
                num_transitions
            );
            return 1;
        }
    } else {
        printf("Failed to define number of transitions in rule file\n");
        return 1;
    }
    rule->transition_length = num_transitions;
    if (!(rule->transitions = malloc(num_transitions * sizeof(Transition_t*)))) {
        printf("Could not allocate memory for ruleset\n");
        return 1;
    }
    for (i = 0; i < num_transitions; i++) {
        if (!(rule->transitions[i] = malloc(sizeof(Transition_t)))) {
            printf("Could not allocate memory for ruleset\n");
            free(rule->transitions);
            return 1;
        }
    }
    if (!(transitions_init(rule->transitions, num_transitions, rule->num_states, infile) == 0)) {
        rule_destroy(rule);
        return 1;
    }
    return 0;
}


int transitions_init(Transition_t** transitions, int num_transitions, unsigned short int num_states, FILE* infile)
{
    int state;
    int end_state;
    int  neighbor_state;
    bool neighbor_negator;
    char line_buffer[80];
    int i;

    for (i = 0; i < num_transitions; i++) {
        if (fscanf(infile, "%80s", line_buffer) == 1) {
        } else {
            fprintf(stderr, "buffer is: %s\n", line_buffer);
            printf("Bad transition mapping in rule file on transition %d\n", i);
            return 1;
        }
        neighbor_negator = false;
        if (sscanf(line_buffer, "%d->%d:%d", &state, &end_state, &neighbor_state) == 3) {
            if ((state < 0 || state > num_states) ||
                (end_state < 0 || end_state > num_states) ||
                (neighbor_state < 0 || neighbor_state > num_states)) {
                printf(
                    "state numbers %d or %d or %d are out of bounds in transition portion rule file\n",
                    state, end_state, neighbor_state
                );
                return 1;
            }
        } else if (sscanf(line_buffer, "%d->%d:~%d", &state, &end_state, &neighbor_state) == 3) {
            if ((state < 0 || state > num_states) ||
                (end_state < 0 || end_state > num_states) ||
                (neighbor_state < 0 || neighbor_state > num_states)) {
                printf(
                    "state numbers %d or %d or %d are out of bounds in transition portion rule file\n",
                    state, end_state, neighbor_state
                );
                return 1;
            }
            neighbor_negator = true;
        } else if (sscanf(line_buffer, "%d->%d", &state, &end_state) == 2) {
            if ((state < 0 || state > num_states) ||
                (end_state < 0 || end_state > num_states)) {
                printf(
                    "state numbers %d or %d are out of bounds in transition portion rule file\n",
                    state, end_state
                );
                return 1;
            }
            neighbor_state = -1;
        } else {
            printf("Bad transition mapping in rule file on transition %d with line %s\n", i + 1, line_buffer);
            printf("Example of format: 0->1 or 0->1:1\n");
            return 1;
        }
        transitions[i]->begin = (unsigned char) state;
        transitions[i]->end = (unsigned char) end_state;
        transitions[i]->neighbor_state = (unsigned char) neighbor_state;
        transitions[i]->negator = neighbor_negator;
        transitions[i]->size = 0;
        if (neighbor_state != -1) {
            fseek(infile, 1, SEEK_CUR);
            if (!(_read_transition_line(infile, &transitions[i]->size, &transitions[i]->transitions) == 0)) {
                printf("Bad transition line in transition number %d\n", i + 1);
                return 1;
            }
        } else {
            transitions[i]->size = 0;
        }
    }
    return 0;
}


/*
 * Reads a line of comma separated integers from a FILE stream
 *
 * Parameters: FILE* file_stream, int* size, int* result
 * Return: Returns 1 for an invalid line; 0 for success
 * Side-Effect: Sets the results list of the values found to result, and the
 *     size of the results to the size provided
 */
int _read_transition_line(FILE* file, int* size, int** results_container)
{
    int current;
    int integer_value = 0;
    int* results;

    *size = 0;
    if (!(results = malloc(sizeof(int)))) {
        return 1;
    }
    while(true) {
        current = fgetc(file);
        if (isdigit(current)) {
            integer_value = integer_value * 10 + (current - '0');
        } else if (current == ',' || current == '\n') {
            results[*size] = integer_value;
            integer_value = 0;
            *size = *size + 1;
            results = realloc(results, *size * sizeof(int));
            if (current == '\n') {
                break;
            }
        } else {
            *size = 0;
            free(results);
            return 1;
        }
    }
    *results_container = results;
    return 0;
}


/*
 * Given a rule, deallocate all dynamically allocated members. Must be called
 * after a rule is no longer needed, and only once
 *
 * Parameters: Rule_t* rule
 * Side-Effects: Deallocates all rule members
 */
void rule_destroy(Rule_t* rule)
{
    int i;

    for (i = 0; i < rule->transition_length; i++) {
        if (rule->transitions[i]->size > 0) {
            free(rule->transitions[i]->transitions);
        }
        free(rule->transitions[i]);
    }
    if (rule->transition_length > 0) {
        free(rule->transitions);
    }
}
