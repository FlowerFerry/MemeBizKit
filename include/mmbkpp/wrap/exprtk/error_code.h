
#ifndef MMBKPP_WRAP_EXPRTK_ERROR_CODE_H_INCLUDED
#define MMBKPP_WRAP_EXPRTK_ERROR_CODE_H_INCLUDED

namespace mmbkpp {
namespace wrap {
namespace exprtk {

enum class error_code : int
{
    ok = 0,
    unknown = -1,
    max_stack_depth_exceeded = -10000,
    empty_expression  = -10010,                     // ERR001 - Empty expression
    empty_lexed_expression = -10020,                // ERR002 - Empty expression after lexer
    invalid_expression = -10030,                    // ERR003 - Invalid expression encountered
    lexer_unknown_error = -10040,
    lexer_generic_token_error = -10041,
    lexer_symbol_error = -10042,
    lexer_number_error = -10043,
    lexer_string_error = -10044,
    lexer_special_function_token_error = -10045,
    mismatched_bracket = -10050,                    // ERR005 - Mismatched brackets
    invalid_numeric_token = -10060,                 // ERR006 - Invalid numeric token
    invalid_1token_sequence = -10070,               // ERR007 - Invalid token sequence
    invalid_3token_sequence = -10080,               // ERR008 - Invalid token sequence (3 tokens)
    invalid_branch_expression  = -10090,            // ERR009 - Invalid expression encountered (branch)
    invalid_syntax = -10100,                        // ERR010 - Invalid syntax, possible missing operator or context
    internal_compilation_check_failed = -10110,     // ERR011 - Internal compilation check failed
    invalid_or_disabled_logic_operation = -10120,   // ERR012 - Invalid or disabled logic operation
    invalid_or_disabled_arithmetic_op = -10130,     // ERR013 - Invalid or disabled arithmetic operation
    invalid_inequality_operation = -10140,          // ERR014 - Invalid inequality operation
    invalid_or_disabled_assignment_op = -10150,     // ERR015 - Invalid or disabled assignment operation
    return_not_allowed_in_subexpr = -10160,         // ERR016 - Return statements cannot be part of sub-expressions
    general_parsing_error = -10170,                 // ERR017 - General parsing error at token
    max_node_depth_exceeded = -10180,               // ERR018 - Expression depth exceeds maximum
    invalid_syntax_non_commutative = -10190,        // ERR019 - Invalid syntax, possible missing operator or context (no commutative)
    variable_node_not_found = -10200,               // ERR020 - Failed to find variable node in symbol table
    invalid_function_param_count = -10210,          // ERR021 - Invalid number of parameters for function
    failed_to_generate_function_call = -10220,      // ERR022 - Failed to generate call to function
    expecting_non_zero_param_function = -10230,     // ERR023 - Expecting ifunction to have non-zero parameter count
    expecting_function_arg_list = -10240,           // ERR024 - Expecting argument list for function
    failed_parse_function_argument = -10250,        // ERR025 - Failed to parse argument for function
    
    // ERR026 - Invalid number of arguments for function
    // ERR027 - Invalid number of arguments for function
    invalid_number_of_arguments_function = -10260, 
    expected_empty_parentheses_function = -10280,   // ERR028 - Expecting '()' for function call
    expected_left_paren_function = -10290,          // ERR029 - Expected '(' at start of function call
    expected_at_least_one_param_function = -10300,  // ERR030 - Expected at least one input parameter for function
    expected_comma_between_args = -10310,           // ERR031 - Expected ',' between function input parameters
    invalid_number_of_function_params = -10320,     // ERR032 - Invalid number of input parameters for function
    no_base_operation_found = -10330,               // ERR033 - No entry found for base operation
    invalid_parameter_count_base_op = -10340,       // ERR034 - Invalid number of input parameters for base op
    expected_comma_if_condition = -10350,           // ERR035 - Expected ',' between if-statement condition and consequent
    failed_parse_consequent_if = -10360,            // ERR036 - Failed to parse consequent for if-statement
    expected_comma_if_consequent = -10370,          // ERR037 - Expected ',' between if-statement consequent and alternative
    failed_parse_alternative_if = -10380,           // ERR038 - Failed to parse alternative for if-statement
    expected_right_paren_if = -10390,               // ERR039 - Expected ')' at the end of if-statement
    failed_synthesize_string_if = -10400,           // ERR040 - Failed to synthesize node: conditional_string
    if_string_branch_type_mismatch = -10410,        // ERR041 - Return types of if-statement differ: string/non-string
    if_vector_branch_type_mismatch = -10420,        // ERR042 - Return types of if-statement differ: vector/non-vector

    // ERR043 - Failed to parse body of consequent for if-statement
    // ERR046 - Failed to parse body of consequent for if-statement
    failed_parse_if_consequent_body = -10430,      
    expected_semicolon_if_consequent = -10440,      // ERR044 - Expected ';' at the end of the consequent for if-statement
    if_expected_semicolon_else = -10450,            // ERR045 - Expected ';' at the end of the consequent for if-statement (else)
    
    // ERR047 - Failed to parse body of the 'else' for if-statement
    // ERR050 - Failed to parse body of the 'else' for if-statement
    failed_parse_else_body = -10470,               
    failed_parse_else_if_body = -10480,             // ERR048 - Failed to parse body of if-else statement
    expected_semicolon_else_if = -10490,            // ERR049 - Expected ';' at the end of the 'else-if' for if-statement
    if_else_string_branch_type_mismatch = -10510,   // ERR051 - Return types of if-statement differ: string/non-string
    if_else_vector_branch_type_mismatch = -10520,   // ERR052 - Return types of if-statement differ: vector/non-vector
    expected_left_paren_if = -10530,                // ERR053 - Expected '(' at start of if-statement
    failed_parse_if_condition = -10540,             // ERR054 - Failed to parse condition for if-statement
    if_statement_invalid = -10550,                  // ERR055 - Invalid if-statement
    ternary_invalid_condition_branch = -10560,      // ERR056 - Invalid condition branch for ternary if-statement
    expected_ternary_question = -10570,             // ERR057 - Expected '?' after condition of ternary if-statement
    failed_parse_ternary_consequent = -10580,       // ERR058 - Failed to parse consequent for ternary if-statement
    expected_ternary_colon = -10590,                // ERR059 - Expected ':' between ternary if-statement consequent and alternative
    failed_parse_ternary_alternative = -10600,      // ERR060 - Failed to parse alternative for ternary if-statement
    ternary_string_branch_type_mismatch = -10610,   // ERR061 - Return types of ternary differ: string/non-string
    ternary_vector_branch_type_mismatch = -10620,   // ERR062 - Return types of ternary differ: vector/non-vector
    logic_not_disabled = -10630,                    // ERR063 - Invalid or disabled logic operation 'not'
    expected_left_paren_while = -10640,             // ERR064 - Expected '(' at start of while-loop condition
    failed_parse_while_condition = -10650,          // ERR065 - Failed to parse condition for while-loop
    expected_right_paren_while = -10660,            // ERR066 - Expected ')' at end of while-loop condition
    failed_parse_while_body = -10670,               // ERR067 - Failed to parse body of while-loop
    failed_synthesize_while = -10680,               // ERR068 - Failed to synthesize while-loop
    failed_synthesize_valid_while = -10690,         // ERR069 - Failed to synthesize 'valid' while-loop
    expected_semicolon_repeat_until = -10700,       // ERR070 - Expected ';' in body of repeat until loop
    failed_parse_repeat_until_body = -10710,        // ERR071 - Failed to parse body of repeat until loop
    expected_left_paren_repeat_until = -10720,      // ERR072 - Expected '(' before condition statement of repeat until loop
    failed_parse_repeat_until_condition = -10730,   // ERR073 - Failed to parse condition for repeat until loop
    expected_right_paren_repeat_until = -10740,     // ERR074 - Expected ')' after condition of repeat until loop
    failed_synthesize_repeat_until = -10750,        // ERR075 - Failed to synthesize repeat until loop
    failed_synthesize_valid_repeat_until = -10760,  // ERR076 - Failed to synthesize 'valid' repeat until loop
    expected_left_paren_for = -10770,               // ERR077 - Expected '(' at start of for-loop
    expected_for_var = -10780,                      // ERR078 - Expected a variable at the start of for-loop initialiser
    expected_var_assignment = -10790,               // ERR079 - Expected variable assignment of initialiser section of for-loop
    for_loop_shadowed_variable = -10800,            // ERR080 - For-loop variable is being shadowed by a previous declaration

    // ERR081 - Failed to add new local variable to SEM
    // ERR184 - Failed to add new local variable to SEM
    // ERR199 - Failed to add new local variable to SEM
    failed_add_local_var_sem = -10810,              
    failed_parse_for_initialiser = -10820,          // ERR082 - Failed to parse initialiser of for-loop
    expected_semicolon_after_for_init = -10830,     // ERR083 - Expected ';' after initialiser of for-loop
    failed_parse_for_condition = -10840,            // ERR084 - Failed to parse condition of for-loop
    expected_semicolon_after_for_cond = -10850,     // ERR085 - Expected ';' after condition section of for-loop
    failed_parse_for_incrementor = -10860,          // ERR086 - Failed to parse incrementor of for-loop
    expected_right_paren_after_for_inc = -10870,    // ERR087 - Expected ')' after incrementor section of for-loop
    failed_parse_for_body = -10880,                 // ERR088 - Failed to parse body of for-loop
    failed_synthesize_valid_for = -10890,           // ERR089 - Failed to synthesize 'valid' for-loop
    expected_switch_keyword = -10900,               // ERR090 - Expected keyword 'switch'
    expected_left_curly_switch = -10910,            // ERR091 - Expected '{' for call to switch statement
    expected_colon_switch_case = -10920,            // ERR092 - Expected ':' for case of switch statement
    expected_semicolon_switch_case = -10930,        // ERR093 - Expected ';' at end of case for switch statement
    multiple_default_cases_switch = -10940,         // ERR094 - Multiple default cases for switch statement
    expected_colon_switch_default = -10950,         // ERR095 - Expected ':' for default of switch statement
    expected_semicolon_switch_default = -10960,     // ERR096 - Expected ';' at end of default for switch statement
    expected_right_curly_switch = -10970,           // ERR097 - Expected '}' at end of switch statement
    expected_multiswitch_token = -10980,            // ERR098 - Expected token '[*]'
    expected_left_curly_multiswitch = -10990,       // ERR099 - Expected '{' for call to [*] statement
    expected_case_multiswitch = -11000,             // ERR100 - Expected a 'case' statement for multi-switch
    expected_colon_case_multiswitch = -11010,       // ERR101 - Expected ':' for case of [*] statement
    expected_semicolon_case_multiswitch = -11020,   // ERR102 - Expected ';' at end of case for [*] statement
    expected_right_curly_multiswitch = -11030,      // ERR103 - Expected '}' at end of [*] statement
    unsupported_vararg_function = -11040,           // ERR104 - Unsupported built-in vararg function
    expected_left_paren_vararg = -11050,            // ERR105 - Expected '(' for call to vararg function
    vararg_zero_parameter_not_allowed = -11060,     // ERR106 - Zero parameter call to vararg function not allowed
    expected_comma_vararg = -11070,                 // ERR107 - Expected ',' for call to vararg function
    expected_left_bracket_string_range = -11080,    // ERR108 - Expected '[' as start of string range definition
    failed_generate_string_range_node = -11090,     // ERR109 - Failed to generate string range node
    failed_synthesize_string_range = -11100,        // ERR110 - Failed to synthesize node: string_range_node
    expected_left_curly_multiseq = -11110,          // ERR111 - Expected '{' for call to multi-sequence
    expected_comma_multiseq = -11120,               // ERR112 - Expected ',' for call to multi-sequence
    expected_left_bracket_range = -11130,           // ERR113 - Expected '[' for start of range
    failed_parse_begin_range = -11140,              // ERR114 - Failed parse begin section of range
    range_lower_bound_less_than_zero = -11150,      // ERR115 - Range lower bound less than zero
    expected_colon_range = -11160,                  // ERR116 - Expected ':' for break in range
    failed_parse_end_range = -11170,                // ERR117 - Failed parse end section of range
    range_upper_bound_less_than_zero = -11180,      // ERR118 - Range upper bound less than zero
    expected_right_bracket_range = -11190,          // ERR119 - Expected ']' for end of range
    invalid_range_constraint = -11200,              // ERR120 - Invalid range, Constraint: r0 <= r1
    unknown_string_symbol = -11210,                 // ERR121 - Unknown string symbol
    string_range_overflow = -11220,                 // ERR122 - Overflow in range for string
    failed_parse_vector_index = -11230,             // ERR123 - Failed to parse index for vector
    expected_right_bracket_vector_index = -11240,   // ERR124 - Expected ']' for index of vector
    symbol_not_a_vector = -11250,                   // ERR125 - Symbol is not a vector
    vector_index_out_of_range = -11260,             // ERR126 - Index out of range for vector

    // ERR127 - Zero parameter call to vararg function not allowed
    // ERR129 - Zero parameter call to vararg function not allowed
    zero_param_vararg_function_not_allowed = -11270,
    expected_comma_vararg_function = -11280,        // ERR128 - Expected ',' for call to vararg function
    vararg_function_min_params = -11300,            // ERR130 - Invalid number of parameters to vararg function (min)
    vararg_function_max_params = -11310,            // ERR131 - Invalid number of parameters to vararg function (max)
    failed_param_type_check_function = -11320,      // ERR132 - Failed parameter type check for function
    param_seq_conflict_function = -11330,           // ERR133 - Function has a parameter sequence conflict
    type_checker_instantiation_failure = -11340,    // ERR134 - Type checker instantiation failure for generic function
    invalid_param_seq_function = -11350,            // ERR135 - Invalid parameter sequence for function
    failed_compile_string_function = -11360,        // ERR136 - Failed to compile string function

    // ERR137 - Zero parameter call to string function not allowed
    // ERR139 - Zero parameter call to string function not allowed
    zero_param_string_function_not_allowed = -11370, 

    // ERR138 - Expected ',' for call to string function
    // ERR142 - Expected ',' for call to string function
    expected_comma_string_function = -11380,       

    // ERR140 - Invalid input parameter sequence for string function
    // ERR141 - Invalid input parameter sequence for string function
    invalid_param_seq_string_function = -11400,    
    invalid_param_seq_overload_function = -11430,   // ERR143 - Invalid input parameter sequence for overloaded function
    invalid_return_type_overload_function = -11440, // ERR144 - Invalid return type for overloaded function
    expected_left_paren_special_func = -11450,      // ERR145 - Expected '(' for special function
    expected_comma_special_func = -11460,           // ERR146 - Expected ',' before next parameter of special function
    
    // ERR147 - Invalid number of parameters for special function
    // ERR148 - Invalid number of parameters for special function
    invalid_num_params_special_func = -11470,      
    invalid_special_function1 = -11490,             // ERR149 - Invalid special function[1]
    invalid_special_function2 = -11500,             // ERR150 - Invalid special function[2]
    break_within_break_not_allowed = -11510,        // ERR151 - Invoking 'break' within a break call is not allowed
    
    // ERR152 - Invalid use of 'break', allowed only in loop
    // ERR155 - Invalid use of 'break', allowed only in loop
    break_only_allowed_in_loop = -11520,           
    failed_parse_break_return_expr = -11530,        // ERR153 - Failed to parse return expression for 'break'
    expected_right_bracket_break = -11540,          // ERR154 - Expected ']' at the completion of break's return expression
    continue_only_allowed_in_loop = -11560,         // ERR156 - Invalid use of 'continue', allowed only in loop
    expected_left_bracket_vector_size = -11570,     // ERR157 - Expected '[' as part of vector size definition
    failed_parse_vector_size = -11580,              // ERR158 - Failed to determine size of vector
    vector_size_must_be_const = -11590,             // ERR159 - Expected a constant literal number as size of vector
    invalid_vector_size = -11600,                   // ERR160 - Invalid vector size
    vector_redefinition = -11610,                   // ERR161 - Illegal redefinition of local vector
    failed_add_local_vector_sem = -11620,           // ERR162 - Failed to add new local vector to SEM
    expected_left_bracket_vector_init = -11630,     // ERR163 - Expected '{' as part of vector initialiser list
    expected_left_curly_vector_init = -11640,       // ERR164 - Expected '{' as part of vector initialiser list
    failed_parse_vector_init1 = -11650,             // ERR165 - Failed to parse first component of vector initialiser
    failed_parse_vector_init2 = -11660,             // ERR166 - Failed to parse second component of vector initialiser
    expected_right_bracket_vector_init = -11670,    // ERR167 - Expected ']' to close single value vector initialiser

    // ERR168 - Expected '{' as part of vector initialiser list
    // ERR169 - Expected '{' as part of vector initialiser list
    failed_parse_vector_init_element = -11680,     
    expected_comma_vector_init = -11700,            // ERR170 - Expected ',' between vector initialisers
    expected_semicolon_vector_def = -11710,         // ERR171 - Expected ';' at end of vector definition
    initializer_list_too_large = -11720,            // ERR172 - Initialiser list larger than number of elements
    failed_generate_vector_init_node = -11730,      // ERR173 - Failed to generate initialisation node for vector
    string_redefinition = -11740,                   // ERR174 - Illegal redefinition of local string variable
    failed_add_local_string_sem = -11750,           // ERR175 - Failed to add new local string variable to SEM
    illegal_var_definition = -11760,                // ERR176 - Illegal variable definition
    expected_var_symbol = -11770,                   // ERR177 - Expected a symbol for variable definition

    // ERR178 - Illegal redefinition of reserved keyword
    // ERR189 - Illegal redefinition of reserved keyword
    illegal_redefinition_reserved_word = -11780,   

    // ERR179 - Illegal redefinition of variable
    // ERR190 - Illegal redefinition of variable
    illegal_redefinition_variable = -11790,        

    // ERR180 - Illegal redefinition of local variable
    // ERR183 - Illegal redefinition of local variable
    // ERR191 - Illegal redefinition of local variable
    // ERR194 - Illegal redefinition of local variable
    // ERR198 - Illegal redefinition of local variable
    illegal_redefinition_local_variable = -11800,  
    failed_parse_var_init_expr = -11810,            // ERR181 - Failed to parse initialisation expression for variable
    expected_semicolon_after_var_def = -11820,      // ERR182 - Expected ';' after variable definition
    illegal_const_var_definition = -11850,          // ERR185 - Illegal const variable definition
    expected_const_var_keyword = -11860,            // ERR186 - Expected 'const' keyword for const-variable definition
    expected_var_keyword_after_const = -11870,      // ERR187 - Expected 'var' keyword for const-variable definition
    expected_symbol_const_var = -11880,             // ERR188 - Expected a symbol for const-variable definition
    failed_parse_const_var_init_expr = -11920,      // ERR192 - Failed to parse initialisation expression for const-variable
    const_var_must_be_constant = -11930,            // ERR193 - const-variable init expr must be a constant/literal
    failed_add_local_const_var_sem = -11950,        // ERR195 - Failed to add new local const-variable to SEM
    expected_curly_for_uninit_var = -11960,         // ERR196 - Expected a '{}' for uninitialised var definition
    expected_semicolon_after_uninit_var = -11970,   // ERR197 - Expected ';' after uninitialised variable
    expected_left_paren_swap = -12000,              // ERR200 - Expected '(' at start of swap statement

    // ERR201 - Expected a symbol for variable or vector element definition
    // ERR205 - Expected a symbol for variable or vector element definition
    expected_symbol_swap_var = -12010,             

    // ERR202 - First parameter to swap is an invalid vector element
    // ERR206 - Second parameter to swap is an invalid vector element
    failed_parse_swap_vector_elem = -12020,        

    // ERR203 - First parameter to swap is an invalid variable
    // ERR207 - Second parameter to swap is an invalid variable
    failed_parse_swap_variable = -12030,           
    expected_comma_swap = -12040,                   // ERR204 - Expected ',' between parameters to swap
    expected_right_paren_swap = -12080,             // ERR208 - Expected ')' at end of swap statement
    return_within_return_not_allowed = -12090,      // ERR209 - Return call within a return call is not allowed
    expected_left_bracket_return = -12100,          // ERR210 - Expected '[' at start of return statement
    expected_comma_return_values = -12110,          // ERR211 - Expected ',' between values during call to return
    zero_param_return_not_allowed = -12120,         // ERR212 - Zero parameter return statement not allowed
    invalid_right_bracket_return = -12130,          // ERR213 - Invalid ']' found during return call
    assert_within_assert_not_allowed = -12140,      // ERR214 - Assert statement within an assert statement is not allowed
    expected_left_paren_assert = -12150,            // ERR215 - Expected '(' at start of assert statement
    failed_parse_assert_condition = -12160,         // ERR216 - Failed to parse condition for assert statement
    expected_comma_assert_message = -12170,         // ERR217 - Expected ',' between condition and message for assert statement
    expected_string_for_assert_message = -12180,    // ERR218 - Expected string for assert message
    expected_comma_assert_id = -12190,              // ERR219 - Expected ',' between message and ID for assert statement
    expected_literal_string_for_assert_id = -12200, // ERR220 - Expected literal string for assert ID
    expected_right_paren_assert = -12210,           // ERR221 - Expected ')' at start of assert statement
    duplicate_assert_id = -12220,                   // ERR222 - Duplicate assert ID
    failed_synthesize_assert = -12230,              // ERR223 - Failed to synthesize assert
    invalid_variable_bracket_sequence = -12240,     // ERR224 - Invalid sequence of variable and bracket
    invalid_bracket_sequence = -12250,              // ERR225 - Invalid sequence of brackets
    failed_to_generate_function_node = -12260,      // ERR226 - Failed to generate node for function
    failed_to_generate_vararg_function_node = -12270,// ERR227 - Failed to generate node for vararg function
    failed_to_generate_generic_function_node = -12280,// ERR228 - Failed to generate node for generic function
    failed_to_generate_string_function_node = -12290,// ERR229 - Failed to generate node for string function
    failed_to_generate_overload_function_node = -12300,// ERR230 - Failed to generate node for overload function
    reserved_symbol_used = -12310,                  // ERR231 - Invalid use of reserved symbol
    failed_to_create_variable = -12320,             // ERR232 - Failed to create variable
    failed_to_resolve_symbol = -12330,              // ERR233 - Failed to resolve symbol
    undefined_symbol = -12340,                      // ERR234 - Undefined symbol
    invalid_syntax_unknown_symbol = -12350,         // ERR235 - Invalid syntax, unknown symbol
    invalid_branches_for_operator = -12360,         // ERR236 - Invalid branches for operator
    failed_generate_node_for_scalar = -12370,       // ERR237 - Failed generate node for scalar
    failed_convert_to_number = -12380,              // ERR238 - Failed to convert to a number
    expected_right_paren = -12390,                  // ERR239 - Expected ')' instead of
    expected_right_bracket = -12400,                // ERR240 - Expected ']' instead of
    expected_right_curly = -12410,                  // ERR241 - Expected '}' instead of
    premature_end_of_expression1 = -12420,          // ERR242 - Premature end of expression[1]
    premature_end_of_expression2 = -12430,          // ERR243 - Premature end of expression[2]
    invalid_branches_received = -12440,             // ERR244 - Invalid branches received for operator
    invalid_branches_for_string_op = -12450,        // ERR245 - Invalid branch pair for string operator
    invalid_branches_for_assignment_op = -12460,    // ERR246 - Invalid branch pair for assignment operator
    invalid_branches_for_break_continue = -12470,   // ERR247 - Invalid branch pair for break/continue operator
    invalid_branches_operator = -12480,             // ERR248 - Invalid branches operator
    invalid_branches_for_string_operator = -12490,  // ERR249 - Invalid branches for string operator
    invalid_branches_for_conditional = -12500,      // ERR250 - Invalid branches for conditional statement
    failed_synthesize_node_conditional = -12510,    // ERR251 - Failed to synthesize node: conditional_node_t
    invalid_branches_for_string_conditional = -12520,// ERR252 - Invalid branches for string conditional statement
    failed_synthesize_node_string_conditional = -12530,// ERR253 - Failed to synthesize node: conditional_string_node_t
    invalid_branches_for_vector_conditional = -12540,// ERR254 - Invalid branches for vector conditional statement
    infinite_while_loop_not_allowed = -12550,       // ERR255 - Infinite loop condition without 'break' or 'return' not allowed in while-loops
    infinite_repeat_until_loop_not_allowed = -12560,// ERR256 - Infinite loop condition without 'break' or 'return' not allowed in repeat-until
    infinite_for_loop_not_allowed = -12570,         // ERR257 - Infinite loop condition without 'break' or 'return' not allowed in for-loop
    failed_synthesize_vararg_function_node = -12580,// ERR258 - Failed to synthesize node: vararg_function_node
    failed_synthesize_generic_function_node = -12590,// ERR259 - Failed to synthesize node: generic_function_node
    failed_synthesize_string_function_node = -12600,// ERR260 - Failed to synthesize node: string_function_node

    // ERR261 - Failed to synthesize node: return_node
    // ERR263 - Failed to synthesize node: return_node
    failed_synthesize_return_node = -12610,        
    failed_synthesize_return_envelope_node = -12620,// ERR262 - Failed to synthesize node: return_envelope_node
    vector_element_index_out_of_range = -12540,     // ERR264 - Index out of range for vector element

    // ERR265 - Failed to synthesize node: vector element
    // ERR266 - Failed to synthesize node: vector element
    failed_synthesize_vector_element_node = -12650,
    cannot_assign_immutable_symbol = -12670,        // ERR267 - Symbol cannot be assigned-to as it is immutable
    cannot_assign_to_const_variable = -12680,       // ERR268 - Cannot assign value to const variable

    // ERR269 - Invalid branches for assignment operator
    // ERR271 - Invalid branches for assignment operator
    invalid_branches_for_assignment_operator = -12690,

    // ERR270 - Failed to synthesize node: assignment_node
    // ERR272 - Failed to synthesize node: assignment_node
    failed_synthesize_assignment_node = -12700,    
    failed_synthesize_node_vector_eq_ineq_logic = -12730,// ERR273 - Failed to synthesize node: vector eq/ineq logic node
    failed_synthesize_node_vector_arithmetic = -12740,// ERR274 - Failed to synthesize node: vector arithmetic node
    failed_synthesize_swap_node = -12750,           // ERR275 - Failed to synthesize node: swap_node
    failed_synthesize_node = -12760,                // ERR276 - Failed to synthesize node
    invalid_function_overload = -12770              // ERR277 - Function is an invalid overload
};

} // namespace exprtk
} // namespace wrap
}

#endif //! MMBKPP_WRAP_EXPRTK_ERROR_CODE_H_INCLUDED
