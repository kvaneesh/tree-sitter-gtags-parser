(struct_specifier name: (type_identifier) @name body:(_)) @definition.class

(declaration type: (union_specifier name: (type_identifier) @name)) @definition.class

(function_declarator declarator: (identifier) @name) @definition.function

(type_definition declarator: (type_identifier) @name) @definition.type

(enum_specifier name: (type_identifier) @name) @definition.type

(pointer_declarator
  declarator: (identifier) @name) @definition.var
(parameter_declaration
  declarator: (identifier) @name) @definition.parameter
(init_declarator
  declarator: (identifier) @name) @definition.var
(array_declarator
  declarator: (identifier) @name) @definition.var
(declaration
  declarator: (identifier) @name) @definition.var

(field_declaration
  declarator: (field_identifier) @name) @definition.field

(preproc_def
  name: (identifier) @name) @definition.macro
