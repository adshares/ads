#include "servers.h"

#include <fstream>

namespace Helper {

Servers::Servers() {
    m_filePath = "";
}

Servers::Servers(const char* filePath)
    : m_filePath(filePath) {
}

void Servers::load(const char* filePath) {
    if (filePath) {
        m_filePath = filePath;
    }

    if (m_filePath.empty()) {
        return;
    }

    std::ifstream file(m_filePath, std::ifstream::in | std::ifstream::binary);
    if (file.is_open()) {
        file.read((char*)&m_header, sizeof(m_header));
        for (unsigned int i=0; i<m_header.nodesCount; ++i) {
            ServersNode node;
            file.read((char*)&node, sizeof(node));
            m_nodes.push_back(node);
        }
        file.close();
    }
}

ServersHeader Servers::getHeader() {
    return m_header;
}

std::vector<ServersNode> Servers::getNodes() {
    return m_nodes;
}

ServersNode Servers::getNode(unsigned int nodeId) {
    if (m_nodes.size() < nodeId){
        throw std::invalid_argument("Node Id over limit");
    }

    return m_nodes.at(nodeId);
}

unsigned int Servers::getNodesCount() {
    return m_nodes.size();
}

}
