-- trivial protocol example
-- declare our protocol
trdp_proto = Proto("TRDP","TRDP Protocol")
comId = ProtoField.uint32("trdp.comId", "comId", base.DEC)
trdp_proto.fields = { comId }
-- create a function to dissect it
function trdp_proto.dissector(buffer,pinfo,tree)
    pinfo.cols.protocol = "TRDP"
    local subtree = tree:add(trdp_proto, buffer(), "TRDP Protocol Data")
    subtree:add(comId, buffer(8,4))
end
-- load the udp.port table
local udp_port = DissectorTable.get("udp.port")
-- register our protocol to handle udp port 7777
udp_port:add(17224,trdp_proto)