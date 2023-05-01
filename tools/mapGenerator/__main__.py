from PIL import Image, ImageDraw
import uuid

MAP_FILE_NAME = "./map_roads.png"
MOTORWAY_COLOUR = (211, 214, 161)

MOTOR_WAY_DISTANCE = 40 ** 2
STREET_DISTANCE = 20 ** 2
DEGENERATE_STEPS = 8
MAX_STREET_DEGEN = 5
MAX_CONNECTIONS = 32

def create_node(pixel_data, index, node_dict, x, y):
    motorway = pixel_data[x, y][0:3] == MOTORWAY_COLOUR
    node = {
        "id": str(uuid.uuid4()),
        "x": x,
        "y": y,
        "motorway": motorway,
        "connections": {},
        "merged": -1
    }
    for xDiff in range(-1, 1):
        for yDiff in range(-1, 1):
            if node_dict.get(x + xDiff, {}).get(y + yDiff):
                connection = node_dict[x + xDiff][y + yDiff]
                node["connections"][connection["id"]] = connection
                connection["connections"][node["id"]] = node
    if not node_dict.get(x):
        node_dict[x] = {}
    node_dict[x][y] = node
    return node

def degenerate(nodes, stage):
    new_nodes = []

    for node in nodes:
        if node["merged"] == stage:
            continue
        elif not node["motorway"] and stage > MAX_STREET_DEGEN:
            new_nodes.append(node)
            continue
        merge_node = None
        for connection in node["connections"].values():
            if (not connection["motorway"] and stage > MAX_STREET_DEGEN):
                continue
            if connection["merged"] < stage:
                merge_node = connection
            break
        if not merge_node:
            new_nodes.append(node)
            continue
        del merge_node["connections"][node["id"]]
        del node["connections"][merge_node["id"]]
        new_node = {
            "id": str(uuid.uuid4()),
            "x": int((merge_node["x"] + node["x"])/2),
            "y": int((merge_node["y"] + node["y"])/2),
            "motorway": merge_node["motorway"] or node["motorway"],
            "connections": merge_node["connections"] | node["connections"],
            "merged": stage
        }
        node["merged"] = stage
        merge_node["merged"] = stage
        for connected_node in new_node["connections"].values():
            if node["id"] in connected_node["connections"]:
                del connected_node["connections"][node["id"]]
            if merge_node["id"] in connected_node["connections"]:
                del connected_node["connections"][merge_node["id"]]
            connected_node["connections"][new_node["id"]] = new_node
        new_nodes.append(new_node)
    return new_nodes

def generate():
    map = Image.open(MAP_FILE_NAME)
    pixel_data = map.load()
    node_dict = {}
    nodes = []
    print(f"Map Texture is {map.size}")
    print("Generating Initial HUGE graph")
    for y in range(map.size[1]):
        for x in range(map.size[0]):
            if not pixel_data[x, y][3]:
                continue
            nodes.append(create_node(pixel_data, len(nodes), node_dict, x, y))

    for i in range(DEGENERATE_STEPS):
        print(f"Degenerating {i} (Nodes: {len(nodes)})")
        nodes = degenerate(nodes, i)
        print(f"Degenerate {i} done. (Now {len(nodes)} nodes)")

    for index, node in enumerate(nodes):
        node["index"] = index

    maxC = 0
    for index, node in enumerate(nodes):
        node["connections"] = list({connection["index"] for connection in node["connections"].values()})
        maxC = max(maxC, len(node["connections"]))
        if len(node["connections"]) > MAX_CONNECTIONS:
            print("OVER MAX CONNECTIONS")
    print(f"Max connections seen was {maxC}")

    print(f"final node count {len(nodes)}")
    new = Image.new(mode="RGB", size=map.size)
    connectedImage = ImageDraw.Draw(new)
    for point in nodes:
        colour = (255, 255, 255)
        if point["motorway"]:
            colour = (255, 0, 0)
        new.putpixel([point["x"], point["y"]], colour)
    new.save("points.png")
    for node in nodes:
        for connection in node["connections"]:
            con = nodes[connection]
            colour = "red"
            if(node["motorway"] and con["motorway"]):
                colour = "yellow"
            connectedImage.line([node["x"], node["y"], con["x"], con["y"]], fill=colour, width=1)

    print("Creating Struct file")
    with open("../../code/nav_mesh_data.h", "w") as f:
        f.write("""#if !defined(NAV_MESH_DATA_H_)
#define NAV_MESH_DATA_H_

nav_mesh_node NAV_MESH_NODES[] = {

    """)
        for node in nodes:
            f.write(f"{{{str(node['x'])}, \n")
            f.write(f"{str(node['y'])}, \n")
            f.write(f"{str(int(node['motorway']))}, \n")
            f.write(f"{str(len(node['connections']))}, \n")
            f.write("{\n")
            for x in range(MAX_CONNECTIONS):
                endChar = ", \n"
                if x == MAX_CONNECTIONS-1:
                    endChar = "\n"
                if x >= len(node["connections"]):
                    f.write(f"  0{endChar}")
                    continue
                else:
                    f.write(f"  {str(node['connections'][x])}{endChar}")
            if nodes[-1]["index"] == node["index"]:
                f.write("}\n")
                f.write("}\n")
            else:
                f.write("}\n")
                f.write("},\n")
        f.write("};\n")
        f.write("nav_mesh NAV_MESH = {\n")
        f.write(f"{str(len(nodes))},\n")
        f.write("NAV_MESH_NODES};\n")
        f.write("#endif\n")
    new.show()
    new.save("navMesh.png")


if __name__ == "__main__":
    generate()
