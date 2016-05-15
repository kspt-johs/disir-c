#ifndef _LIBDISIR_DISIR_PRIVATE_H
#define _LIBDISRI_DISIR_PRIVATE_H

#include <disir/disir.h>
#include <disir/io.h>

//! \brief The main libdisir instance structure. All I/O operations requires an instance of it.
struct disir
{
    struct disir_input      *dio_input_queue;
    struct disir_output     *dio_output_queue;

    struct disir_mold     *internal_mold;
};

//! \brief The internal input plugin structure.
struct disir_input
{
    //! String identifier of the input type.
    char                            *di_type;
    //! String description of the input type.
    char                            *di_description;

    //! Input structure holds all function callbaks to perform the input for this plugin.
    struct disir_input_plugin       di_input;

    //! linked list pointers
    struct disir_input *next, *prev;
};

//! \brief The internal output plugin structure.
struct disir_output
{
    //! String identifier of the output type.
    char                            *do_type;
    //! String description of the output type.
    char                            *do_description;

    //! Output structure holds all function callbacks to perform the output for this plugin.
    struct disir_output_plugin      do_output;

    struct disir_output *next, *prev;
};

//! \brief Allocate a disir_output structure
struct disir_output * dx_disir_output_create (void);

//! \brief Destroy a previously allocated disir_output structure
enum disir_status dx_disir_output_destroy (struct disir_output **output);

//! \brief Allocate a disir_input structure
struct disir_input * dx_disir_input_create (void);

//! \brief Destroy a previously allocated disir_input structure
enum disir_status dx_disir_input_destroy (struct disir_input **output);

// TMP - REWORK
enum disir_status
dio_register_ini (struct disir *disir);


#endif // _LIBDISIR_DISIR_PRIVATE_H

