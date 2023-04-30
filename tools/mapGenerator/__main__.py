"""It's me writing python wow // Matt"""
import ctypes
from PIL import Image, ImageDraw

GROUP_LOCALITY = 5 ** 2
CONNECTION_LOCALITY = 10 ** 2
MAX_CONNECTIONS = 12


def find_local_point(groups, x, y):
    for group in groups:
        xDiff = (group[0] - x)
        yDiff = (group[1] - y)
        if  xDiff*xDiff + yDiff*yDiff >= GROUP_LOCALITY:
            continue
        return group
    return None


def generate():
    map = Image.open("./testMap.png")
    pixel_data = map.load()

    print(f"Map texture is {map.size}")
    print("Generating points")
    points = []
    for y in range(map.size[1]):
        for x in range(map.size[0]):
            if not pixel_data[x, y][3]:
                continue
            points.append((x, y))

    print(f"Points created, squashing nearby points (Size {GROUP_LOCALITY})")
    groups = []
    for point in points:
        if find_local_point(groups, point[0], point[1]) is None:
            groups.append(point)
    print(f"{len(groups)} nodes after squash.")
    print("Creating connections")

    nodes = []
    for index, point in enumerate(groups):
        node = {
            "index": index,
            "x": point[0],
            "y": point[1],
            "connections": []
        }
        nodes.append(node)
    max_con = 0
    for node in nodes:
        for checkNode in nodes:
            if node["index"] == checkNode["index"]:
                continue
            xDiff = (node["x"] - checkNode["x"])
            yDiff = (node["y"] - checkNode["y"])
            if xDiff*xDiff + yDiff*yDiff > CONNECTION_LOCALITY:
                continue
            node["connections"].append(checkNode["index"])
        max_con = max(max_con, len(node["connections"]))
    print(f"Done, Max connections seen {max_con}")
    if max_con > MAX_CONNECTIONS:
        print("MAX CONNECTIONS EXCEEDED, UPDATE STRUCT TO MATCH")
        return

    new = Image.new(mode="RGB", size=map.size)
    connectedImage = ImageDraw.Draw(new)
    for point in groups:
        new.putpixel([point[0], point[1]], (255, 255, 255))
    for node in nodes:
        for connection in node["connections"]:
            con = nodes[connection]
            connectedImage.line([node["x"], node["y"], con["x"], con["y"]], fill="yellow", width=1)
    new.show()
    new.save("navMesh.png")

    print("Creating Struct file")
    with open("../../code/nav_mesh_data.h", "w") as f:
        f.write("""#if !defined(NAV_MESH_DATA_H_)
#define NAV_MESH_DATA_H_

nav_mesh_node NAV_MESH_NODES[] = {

    """)
        for node in nodes:
            f.write(f"{{{str(node['x'])}, \n")
            f.write(f"{str(node['y'])}, \n")
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



if __name__ == "__main__":
    generate()
