from typing import Literal
from pydantic import BaseModel, Field


class NicLink(BaseModel):
    """One PDES NIC of a node: NIC i of this node is wired to NIC `peer_nic` of node `peer_node`."""
    peer_node: int = Field(description="node_number of the machine this NIC is wired to (this node's own number for a self-link).")
    peer_nic: int = Field(default=0, description="Index of the NIC on peer_node this NIC is wired to.")
    dev: Literal['e1000', 'virtio-net-pci'] = Field(description="Guest-facing NIC model.")
    latency_ns: int = Field(description="One-way wire latency in nanoseconds (also the MNQ on this link).")
    sync: bool = Field(description="Require WWT sync messages on this link.")

    @classmethod
    def from_cli(cls, s: str) -> dict:
        """CLI spelling of one entry: peer_node:peer_nic:dev:latency_ns:sync (e.g. 1:0:virtio-net-pci:100000:true)."""
        peer_node, peer_nic, dev, latency_ns, sync = s.split(":")
        # a plain dict so the value also survives the OmegaConf merge of YAML + CLI overrides; the factory builds the model
        return cls(peer_node=int(peer_node), peer_nic=int(peer_nic), dev=dev, latency_ns=int(latency_ns), sync=sync.lower() == "true").model_dump()
