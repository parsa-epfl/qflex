from typing import Annotated, List, Dict
import functools


from .typer_base import TyperDataClassMeta
import inspect





# important TODO make a test for this class later
def data_class_wrap(*args):
    
    root_names = set()
    params = []
    nested_params_names_nested = set()
    
    root_name_to_init_funciton: Dict[str, callable] = {}
    root_name_to_params: Dict[str, List[inspect.Parameter]]={}
    
    for arg in args:
        assert isinstance(arg, TyperDataClassMeta), "All arguments must be instances of TyperDataClassMeta"
        assert arg.name not in root_names, f"Data class name {arg.name} is duplicated in multiple data classes"
        root_names.add(arg.name)
        curr_params: List[inspect.Parameter] = []
        for name, param in inspect.signature(arg.init_function).parameters.items():
            assert name not in nested_params_names_nested, f"Parameter name {name} is duplicated in multiple data classes"
            params.append(param)
            curr_params.append(param)
            nested_params_names_nested.add(name)
        root_name_to_init_funciton[arg.name] = arg.init_function 
        root_name_to_params[arg.name] = curr_params
            

    def func_wrapper(func):

        # TODO add check so no one can have args
        @functools.wraps(func)
        def wrapper(
            *args,
            **kwargs,
        ):
            print(root_names)
            for root_name, init_function in root_name_to_init_funciton.items():
                
                # Create a dictionary of the parameters that are needed for the init function
                selected_kwargs ={}
                
                for nested_param in root_name_to_params[root_name]:
                    # TODO double check this
                    nested_param_name = nested_param.name
                    
                    if nested_param_name in kwargs:
                        selected_kwargs[nested_param_name] = kwargs[nested_param_name]
                    else:
                        if nested_param.default is not inspect.Parameter.empty:
                            # TODO create a test for this and also check it doesn't apply when value is given
                            selected_kwargs[nested_param_name] = nested_param.default
                        else:
                            raise ValueError(f"Missing required parameter {nested_param_name} for {root_name}")

                for nested_param_name in selected_kwargs.keys():
                    kwargs.pop(nested_param_name)
                
                value = init_function(**selected_kwargs)
                kwargs[root_name] = value

            return func(*args, **kwargs)

        # TODO this is a simple solution, check if typer uses other attributes or not
        # TODO make it more robust for the *args and **kwargs
        func_params = [param for name, param in inspect.signature(func).parameters.items() if name not in root_names]

        # TODO this order of appending always thinks the values coming from composites are none defaults
        params_final = list(params)+func_params
        wrapper.__signature__ = inspect.Signature(parameters=params_final)


        return wrapper

    return func_wrapper




