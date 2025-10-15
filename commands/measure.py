from commands import Executor
"""parser.add_argument(
        "--core-info-path",
        default="./run/core_info.csv",
        help="Path to existing core_info.csv file. If exists, will automatically generate core_info_new.csv (default: ./run/core_info.csv)"
    )
    parser.add_argument(
        "-u", "--unit-size",
        type=int,
        default=1,
        help="Size of sampling unit in terms of INTERVAL (default: 1)"
    )
    parser.add_argument(
        "-i", "--index",
        type=int,
        default=2,
        help="Index of the sampling unit to analyze (in terms of number of INTERVAL)"
    )
    parser.add_argument(
        '-g', '--core-groups',
        help='Specify core groups to analyze separately (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are analyzed as one group.'
    )
    parser.add_argument(
        '--confidence',
        type=float,
        help="Confidence level for sample size calculation (optional, default: 95.0)"
    )
    parser.add_argument(
        '--error',
        type=float,
        help="Acceptable sampling error (optional, default: 0.05)"
    )
    parser.add_argument(
        '--plot',
        action='store_true',
        default=True,
        help="Plot U-IPC distribution using plotext (default: True)"
    )
    parser.add_argument(
        '--no-plot',
        dest='plot',
        action='store_false',
        help="Disable plotting of U-IPC distribution"
    )"""


class FunctionalWarming(Executor):
    """
    This class handles the functional warming phase of the experiment and generates checkpoints.
    """

    def __init__(
        self,
        unit_size: int = 1,
        sampling_index: int = 2,
        core_groups: str = '',
        confidence: float = 95.0,
        error: float = 0.05,
        plot: bool = True,
    ):
        self.unit_size = unit_size
        self.sampling_index = sampling_index
        self.core_groups = core_groups
        self.confidence = confidence
        self.error = error
        self.plot = plot
    
    def cmd(self) -> str:
        return f'./result.py \
        --unit-size {self.unit_size} \
        --index {self.sampling_index} \
        --core-groups "{self.core_groups}" \
        --confidence {self.confidence} \
        --error {self.error} \
        {"--plot" if self.plot else "--no-plot"}'
    

