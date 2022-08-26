#include <cstdio>
#include <string.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>

using namespace std;

// Estrutura de uma entrada
typedef struct
{
    unsigned char tipo; // 0x10 para sub-diretorio e 0x20 para arquivo
    char nome[22];
    char extensao[3];
    unsigned short primeiro_cluster;
    unsigned int tamanho;
}
__attribute__((packed)) Entry;

typedef struct
{
    unsigned short bytes_por_setor;
    unsigned char setores_por_cluster;
    unsigned short entradas_root;
    unsigned int setores_volume_logico;
    unsigned short setores_fat;
    unsigned char status_ultimo_format;
} __attribute__((packed)) BootRecord;

typedef struct
{
    BootRecord bootRecord;
    char empty[500];

} __attribute__((packed)) BootRecordSector;

void menu();

#define BYTES_PER_SECTOR 512
#define SECTOR_PER_CLUSTER 1
#define BYTES_PER_CLUSTER (BYTES_PER_SECTOR * SECTOR_PER_CLUSTER)
#define START_FAT 1
#define START_ROOT_DIR 257

BootRecord bootRecord;
int dataStart;
FILE *file;

int totalFreeSectors()
{ // retorna a quantidade de setores livres na fat
    unsigned short int value;
    int freeSectors = 0;
    fseek(file, START_FAT * BYTES_PER_CLUSTER, SEEK_SET);
    for (int i = 0; i < 65536; i++)
    {
        fread(&value, sizeof(unsigned short int), 1, file);
        if (value == 0xFFFF)
        {
            freeSectors++;
        }
    }
    return freeSectors;
}

void setOnFAT(unsigned short int value, int pos)
{
    fseek(file, (START_FAT * BYTES_PER_CLUSTER) + (pos * 2), SEEK_SET);
    fwrite(&value, sizeof(unsigned short int), 1, file);
}

unsigned short int searchInFAT(int pos)
{
    unsigned short int aux;
    fseek(file, (START_FAT * BYTES_PER_CLUSTER) + (pos * 2), SEEK_SET);
    fread(&aux, sizeof(unsigned short int), 1, file);
    return aux;
}

void openImage()
{
    file = fopen("imagem.img", "rb+");
    if (file == NULL)
    {
        cout << "! ERRO - Imagem não existe" << endl;
        exit(0);
    }
    fread(&bootRecord, sizeof(BootRecord), 1, file);
    fseek(file, 0, SEEK_SET);
}

void printHelper(Entry entry, int depth)
{
    cout << "|  ";
    for (int i = 1; i <= depth; i++)
    {
        if (i == depth)
            cout << "|__";
        else
            cout << "   ";
    }
    if (entry.tipo == 0x20)
    {
        cout << entry.nome << "." << entry.extensao << "   "
             << "FILE"
             << "    " << entry.tamanho << " bytes ";
    }
    else if (entry.tipo == 0x10)
    {
        cout << entry.nome << "   "
             << "DIR"
             << "    " << entry.tamanho << " bytes ";
    }
    cout << endl;
}

void listFiles(int currentSector, bool listFiles, bool listDirectories)
{
    Entry currentEntry;
    int entries = 0;
    currentEntry.tipo = 0x20;

    do
    {
        fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);

        while (currentEntry.tipo != 0 && entries < bootRecord.entradas_root)
        {
            fread(&currentEntry, sizeof(Entry), 1, file);

            printHelper(currentEntry, 0);

            if (currentEntry.tipo == 0)
            {
                entries++;
                break;
            }
            entries++;
        }

        if (searchInFAT(currentSector) != 0xFFFE)
        {
            currentSector = searchInFAT(currentSector);
            entries = 0;
            continue;
        }
        else
        {
            break;
        }
    } while (searchInFAT(currentSector) != 0xFFFE);
}

void format()
{
    int nSectors = 0;
    cout << "| Digite a quantidade de setores a serem formatados (0 a 65536): ";
    cin >> nSectors;

    if (nSectors < 0 || nSectors > 65536)
    {
        cout << "! Numero de setores invalido... \n"
             << endl;
        menu();
    }

    int rootDirSize = 0;
    cout << "| Digite o número de entradas do diretório raiz (múltiplo de 16): ";
    cin >> rootDirSize;

    if (rootDirSize % 16 != 0)
    {
        cout << "! O número de entradas deve ser múltiplo de 16... \n"
             << endl;
        menu();
    }

    int rootDirSectors = rootDirSize / 16;
    unsigned short zero = 0;
    unsigned short end = 0xFFFE;
    unsigned short free = 0xFFFF;
    unsigned short reserved = 0xFFFD;
    dataStart = START_ROOT_DIR + rootDirSectors;

    // montar o boot record
    bootRecord.bytes_por_setor = BYTES_PER_SECTOR;
    bootRecord.setores_por_cluster = 1;
    bootRecord.entradas_root = rootDirSize;
    bootRecord.setores_volume_logico = (nSectors * (BYTES_PER_SECTOR) / 2);
    bootRecord.setores_fat = (65536 * 2) / (BYTES_PER_CLUSTER);
    bootRecord.status_ultimo_format = 0;

    BootRecordSector bootRecordSector;
    bootRecordSector.bootRecord = bootRecord;
    for (int i = 0; i < 500; i++)
    {
        bootRecordSector.empty[i] = 0;
    }

    unsigned short fatSector[256];
    unsigned short dataSector[256];
    for (int k = 0; k < 256; k++)
    {
        fatSector[k] = 0xFFFF;
        dataSector[k] = 0x0000;
    }

    int reservedEntries = 0; // 1 do Boot Record, 256 da FAT
    int rootDirEntries = rootDirSectors;
    // formatação
    for (int i = 0; i < nSectors; i++) // 65.536 setores
    {
        if (i == 0)
        {
            // boot record
            fwrite(&bootRecord, sizeof(BootRecordSector), 1, file);
        }
        else if (i <= bootRecord.setores_fat)
        {
            if (reservedEntries < START_ROOT_DIR)
            {
                for (int k = 0; k < 256; k++)
                {
                    if (reservedEntries < START_ROOT_DIR)
                    {
                        fwrite(&reserved, sizeof(reserved), 1, file);
                        reservedEntries++;
                    }
                    else
                    {
                        if (rootDirEntries > 0)
                        {
                            fwrite(&end, sizeof(free), 1, file);
                            rootDirEntries--;
                        }
                        else
                        {
                            fwrite(&free, sizeof(free), 1, file);
                        }
                    }
                }
            }
            else
            {
                fwrite(&fatSector, sizeof(fatSector), 1, file);
            }
        }
        else if (i <= bootRecord.setores_fat + rootDirSectors)
        {
            for (int k = 0; k < 16; k++)
            {
                fwrite(&zero, sizeof(Entry), 1, file);
            }
        }
        else
        {
            fwrite(&dataSector, sizeof(dataSector), 1, file);
        }
    }
}

int getFileSize(const std::string &fileName) // retorna o tamanho do arquivo
{
    ifstream file(fileName.c_str(), ifstream::in | ifstream::binary | ifstream::ate);

    if (!file.is_open())
    {
        return -1;
    }

    int fileSize = file.tellg();
    file.close();

    return fileSize;
}

int searchFreeSector()
{
    unsigned short int entry;
    // começa a busca a partir do cluster 1, no início da FAT
    fseek(file, BYTES_PER_CLUSTER, SEEK_SET);
    for (int i = 0; i < 65536; i++)
    {
        fread(&entry, 2, 1, file);
        if (entry == 0xFFFF)
            return i;
    }
    return -1;
}

void fileToFS(int cluster)
{
    Entry entry;
    char buffer[512];
    unsigned short int one = 1;
    char filePath[1000];

    cout << "| Digite o caminho completo do arquivo a ser copiado:" << endl;
    cout << "| ";
    fscanf(stdin, "%s", filePath);

    FILE *arq;
    arq = fopen(filePath, "rb+"); // tenta abrir o arquivo
    if (arq == NULL)
    {
        cout << "! ERRO - Caminho invalido" << endl;
        menu();
    }

    entry.tipo = 32; // 0x20 = arquivo
    fseek(arq, 0, SEEK_END);
    entry.tamanho = getFileSize(filePath); // retorna o tamanho do arquivo
    char bar = '/';
    char *ret;
    ret = strrchr(filePath, bar); // procura a ultima ocorrencia do caracter barra
    ret++;                        // avanca a ultima barra encontrada
    string extensao;
    string nome;

    int ext = 0;
    for (int k = 0; k < 22; k++)
    {
        if (ret[k] == '.')
        {
            ext = k + 1;
            break;
        }
        entry.nome[k] = ret[k];
    }
    if (ext != 0)
    {
        for (int k = 0; k < 3; k++)
        {
            entry.extensao[k] = ret[ext + k];
        }
    }

    entry.primeiro_cluster = searchFreeSector(); // procura um setor livre

    int nSectors;
    nSectors = ceil((double)entry.tamanho / BYTES_PER_CLUSTER);

    if (nSectors > totalFreeSectors())
    {
        cout << "! Espaco insuficiente" << endl;
        fclose(arq);
        return;
    }

    Entry temp;
    int nEntries = 0;

    fseek(arq, 0, SEEK_SET);
    fseek(file, cluster * BYTES_PER_CLUSTER, SEEK_SET);

    while (fread(&temp, sizeof(Entry), 1, file))
    {
        if ((temp.tipo == 16 || temp.tipo == 32) && nEntries < bootRecord.entradas_root)
        {
            if (!strncmp(temp.nome, entry.nome, 22))
            {
                cout << "! Arquivo existente no sistema" << endl;
                return;
            }
            nEntries++;
        }
        else
        {
            if (nEntries != bootRecord.entradas_root)
            {
                fseek(file, (cluster * BYTES_PER_CLUSTER) + (nEntries * sizeof(Entry)), SEEK_SET);
                fwrite(&entry, sizeof(Entry), 1, file);
                break;
            }
            else
            {
                cout << "! O diretório está cheio" << endl;
                return;
            }
        }
    }
    fflush(file);

    int currentSector = entry.primeiro_cluster;
    int newSector = 0;

    fseek(arq, 0, SEEK_SET);
    int size = entry.tamanho % (BYTES_PER_CLUSTER);
    for (nSectors; nSectors >= 1; nSectors--)
    {
        if (nSectors == 1)
        {
            char tempBuffer[size];
            fread(&tempBuffer, size, 1, arq);
            fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
            fwrite(&tempBuffer, size, 1, file);
            setOnFAT(0xFFFE, currentSector);
        }
        else
        {
            fread(&buffer, BYTES_PER_CLUSTER, 1, arq);
            fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
            fflush(file);
            fwrite(&buffer, BYTES_PER_CLUSTER, 1, file);
            setOnFAT(currentSector, currentSector); // se pa da pra tirar
            newSector = searchFreeSector();
            setOnFAT(newSector, currentSector);

            // newSector = searchFreeSector();
            currentSector = newSector;
        }
    }
    cout << "| Arquivo copiado com sucesso" << endl;

    fclose(arq);
    return;
}

void selectFileToFSDestination()
{
    int currentDir = START_ROOT_DIR;
    char dest[1000];
    bool found = false;

    Entry currentEntry;
    int entries = 0;

    while (!found)
    {
        if (currentDir == START_ROOT_DIR)
        {
            currentEntry.tipo = 0x10;
            strcpy(currentEntry.nome, ".");
            currentEntry.tamanho = 0;
            printHelper(currentEntry, 0);
        }
        listFiles(currentDir, false, true);
        // input
        cout << "| Digite o destino da página:" << endl;
        cout << "| ";
        fscanf(stdin, "%s", dest);

        if (!strcmp(dest, "."))
        { // escreve no root
            fileToFS(currentDir);
            found = true;
        }
        else
        {
            // encontra o cluster do sub-diretório
            fseek(file, currentDir * BYTES_PER_CLUSTER, SEEK_SET);
            while (currentEntry.tipo != 0 && entries < bootRecord.entradas_root)
            {
                fread(&currentEntry, sizeof(Entry), 1, file);
                if (currentEntry.tipo == 0x10 && !strcmp(currentEntry.nome, dest))
                {
                    currentDir = currentEntry.primeiro_cluster;
                    break;
                }
            }
        }
    }
}

void fileToDisk(Entry entry)
{
    FILE *dest;
    char origin[1000], destiny[1000], buffer[2048];
    unsigned short int currentSector = entry.primeiro_cluster;
    Entry currentEntry = entry;

    strcat(origin, entry.nome);
    cout << origin << endl;
    cout << "| Digite o caminho destino:" << endl;
    cout << "| ";
    cin >> destiny;

    strcat(destiny, "/");
    strcat(destiny, origin);
    dest = fopen(destiny, "wb+");

    fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);

    int resto = currentEntry.tamanho % BYTES_PER_CLUSTER;
    int nSetores = ceil((double)currentEntry.tamanho / (BYTES_PER_CLUSTER));
    for (nSetores; nSetores > 0; nSetores--)
    {
        if (nSetores == 1)
        {
            char tempBuffer[resto];

            fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
            int read = fread(&tempBuffer, resto, 1, file);
            int write = fwrite(&tempBuffer, resto, 1, dest);
          
            currentSector = searchInFAT(currentSector);
        }
        else
        {
            fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
            int read = fread(&buffer, BYTES_PER_CLUSTER, 1, file);
            int write = fwrite(&buffer, BYTES_PER_CLUSTER, 1, dest);
        
            currentSector = searchInFAT(currentSector);
        }
    }

    fclose(dest);

    cout << "| Arquivo exportado com sucesso" << endl;
    return;

    if (searchInFAT(currentSector == 0xFFFE))
    {
        for (int i = 0; i < bootRecord.entradas_root; i++)
        {
            fseek(file, (currentSector * BYTES_PER_CLUSTER) + (i * sizeof(Entry)), SEEK_SET);
            fread(&currentEntry, sizeof(Entry), 1, file);
            if (currentEntry.tipo != 0x20 && currentEntry.tipo != 0x10)
            {
                fclose(dest);
                cout << "! Falha na exportacao do arquivo" << endl;
                return;
            }
            if (!strncmp(origin, currentEntry.nome, 22))
            {
                currentSector = currentEntry.primeiro_cluster;
                int resto = currentEntry.tamanho % BYTES_PER_CLUSTER;
                char tempBuffer[resto];
                int nSetores = ceil((double)currentEntry.tamanho / (BYTES_PER_CLUSTER));
                for (nSetores; nSetores > 0; nSetores--)
                {
                    if (nSetores == 1)
                    {

                        fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
                        int read = fread(&tempBuffer, resto, 1, file);
                        int write = fwrite(&tempBuffer, resto, 1, dest);
                       
                        currentSector = searchInFAT(currentSector);
                    }
                    else
                    {
                        fseek(file, currentSector * BYTES_PER_CLUSTER, SEEK_SET);
                        int read = fread(&buffer, BYTES_PER_CLUSTER, 1, file);
                        int write = fwrite(&buffer, BYTES_PER_CLUSTER, 1, dest);
                       
                        currentSector = searchInFAT(currentSector);
                    }
                }
                fclose(dest);
                cout << "| Arquivo exportado com sucesso" << endl;
                return;
            }
        }
    }
}

void selectFileToDiskDestination()
{
    int currentDir = START_ROOT_DIR;
    char dest[1000];
    bool found = false;

    Entry currentEntry;
    int entries = 0;

    while (!found)
    {
        if (currentDir == START_ROOT_DIR)
        {
            currentEntry.tipo = 0x10;
            strcpy(currentEntry.nome, ".");
            currentEntry.tamanho = 0;
            printHelper(currentEntry, 0);
        }
        listFiles(currentDir, true, true);
        // input
        cout << "| Digite o nome do arquivo a ser copiado (insira apenas o nome, sem a extensão):" << endl;
        cout << "| ";
        fscanf(stdin, "%s", dest);

        // encontra o cluster do sub-diretório
        fseek(file, currentDir * BYTES_PER_CLUSTER, SEEK_SET);
        while (currentEntry.tipo != 0 && entries < bootRecord.entradas_root)
        {
            fread(&currentEntry, sizeof(Entry), 1, file);
            if (currentEntry.tipo == 0x10 && !strcmp(currentEntry.nome, dest))
            {
                currentDir = currentEntry.primeiro_cluster;
            }
            else if (currentEntry.tipo == 0x20 && !strcmp(currentEntry.nome, dest))
            {
                found = true;
                fileToDisk(currentEntry);
                break;
            }
        }
    }
}

void mkDir(int cluster, int parent)
{
    unsigned short end = 0xFFFE;
    char dirName[22];
    Entry newEntry;
    Entry auxEntry;

    cout << "| Digite o nome do novo diretorio: ";
    scanf("%s", dirName);

    newEntry.tipo = 0x10;
    strncpy(newEntry.nome, dirName, 22);
    newEntry.primeiro_cluster = searchFreeSector();
    setOnFAT(end, newEntry.primeiro_cluster);
    newEntry.tamanho = 0;

    while (searchInFAT(cluster) != 0xFFFE)
    {
        cluster = searchInFAT(cluster);
        fseek(file, cluster * BYTES_PER_CLUSTER, SEEK_SET);
        for (int i = 0; i < bootRecord.entradas_root; i++)
        {
            fread(&auxEntry, sizeof(Entry), 1, file);
            if (!strncmp(auxEntry.nome, newEntry.nome, 22))
            {
                cout << "! Diretorio ja existente" << endl;
                return;
            }
        }
    }

    fseek(file, cluster * BYTES_PER_CLUSTER, SEEK_SET);

    for (int i = 0; i <= bootRecord.entradas_root; i++)
    {
        if (i == bootRecord.entradas_root)
        {
            setOnFAT(searchFreeSector(), cluster);
            fseek(file, searchInFAT(cluster) * sizeof(unsigned short int), SEEK_SET);
            fwrite(&end, sizeof(unsigned short int), 1, file);
            fseek(file, searchInFAT(cluster) * BYTES_PER_CLUSTER, SEEK_SET);
            fwrite(&newEntry, sizeof(Entry), 1, file);
        }
        fseek(file, (cluster * BYTES_PER_CLUSTER) + (i * sizeof(Entry)), SEEK_SET);
        fread(&auxEntry, sizeof(Entry), 1, file);
        fflush(file);

        if (!strncmp(auxEntry.nome, newEntry.nome, 22))
        {
            cout << "! Diretorio ja existente" << endl;
            return;
        }
        if (auxEntry.tipo != 0x10 && auxEntry.tipo != 0x20)
        {
            fseek(file, (cluster * BYTES_PER_CLUSTER) + (i * sizeof(Entry)), SEEK_SET);
            fwrite(&newEntry, sizeof(Entry), 1, file);
            break;
        }
    }

    fseek(file, newEntry.primeiro_cluster * BYTES_PER_CLUSTER, SEEK_SET);

    // cria entry '.'
    Entry currentDir;
    currentDir.tipo = 0x10;
    strcpy(currentDir.nome, ".");
    currentDir.primeiro_cluster = newEntry.primeiro_cluster;
    currentDir.tamanho = 0;

    // cria entry '..'
    Entry parentDir;
    parentDir.tipo = 0x10;
    strcpy(parentDir.nome, "..");
    parentDir.primeiro_cluster = parent;
    parentDir.tamanho = 0;

    fwrite(&currentDir, sizeof(Entry), 1, file);
    fwrite(&parentDir, sizeof(Entry), 1, file);
    return;
}

void selectMkDirDestination()
{
    int currentDir = START_ROOT_DIR;
    int lastDir = START_ROOT_DIR;
    char dest[1000];
    bool found = false;
    int pathLevel = 0;
    Entry currentEntry;
    int entries = 0;
    currentEntry.tipo = 0x10;
    strcpy(currentEntry.nome, ".");
    currentEntry.tamanho = 0;

    int path[10];
    for (int i = 0; i < 10; i++)
    {
        path[i] = 0;
    }
    path[0] = START_ROOT_DIR;

    cout << "| Digite o destino do diretório (o diretório corrente pode ser acessado pelo símbolo '.') :" << endl;

    while (!found)
    {

        if (currentDir == START_ROOT_DIR)
        {
            currentEntry.tipo = 0x10;
            strcpy(currentEntry.nome, ".");
            currentEntry.tamanho = 0;
            printHelper(currentEntry, 0);
        }
        listFiles(currentDir, false, true);
        // input
        cout << "| Diretório atual: " << currentEntry.nome << endl;
        cout << "| >> ";
        fscanf(stdin, "%s", dest);

        if (!strcmp(dest, "."))
        { // escreve no root
            found = true;
            mkDir(currentDir, lastDir);
        }
        else if (!strcmp(dest, ".."))
        {
            fseek(file, currentDir * BYTES_PER_CLUSTER, SEEK_SET);
            while (currentEntry.tipo != 0 && entries < bootRecord.entradas_root)
            {
                fread(&currentEntry, sizeof(Entry), 1, file);
                if (currentEntry.tipo == 0x10 && !strcmp(currentEntry.nome, dest))
                {
                    path[pathLevel] = 0;
                    if (pathLevel > 0)
                        pathLevel--;

                    // funciona
                    currentDir = path[pathLevel];
                    lastDir = path[pathLevel - 1];
                    break;
                }
            }
        }
        else
        {
            bool found = false;
            // encontra o cluster do sub-diretório
            fseek(file, currentDir * BYTES_PER_CLUSTER, SEEK_SET);
            while (currentEntry.tipo != 0 && entries < bootRecord.entradas_root)
            {
                fread(&currentEntry, sizeof(Entry), 1, file);
                if (currentEntry.tipo == 0x10 && !strcmp(currentEntry.nome, dest))
                {
                    pathLevel += 1;
                    if (pathLevel > 10)
                    {
                        cout << "! Não é possível criar outro subdiretório" << endl;
                        break;
                    }

                    path[pathLevel] = currentEntry.primeiro_cluster;
                    lastDir = path[pathLevel - 1];
                    currentDir = path[pathLevel];
                    found = true;
                    break;
                }
            }
            if (!found) {
                cout << "O diretório não foi encontrado" << endl;
            }
        }
    }
}

void printEntry(Entry entry, int depth)
{
    if (entry.tipo != 0x10 && entry.tipo != 0x20)
        return;

    printHelper(entry, depth);

    if (entry.tipo == 0x10)
    {
        Entry curr;
        curr.tipo = 0x10;
        int n = 2;
        int dirCluster = entry.primeiro_cluster;
        while (curr.tipo != 0 && n < bootRecord.entradas_root)
        {
            fseek(file, (dirCluster * BYTES_PER_CLUSTER) + (n * sizeof(Entry)), SEEK_SET);
            fread(&curr, sizeof(Entry), 1, file);
            printEntry(curr, depth + 1);
            n++;
        }
    }
}

void listAllFiles()
{
    Entry curr;
    curr.tipo = 0x10;
    int n = 0;
    while (n < bootRecord.entradas_root)
    {
        fseek(file, (START_ROOT_DIR * BYTES_PER_CLUSTER) + (n * sizeof(Entry)), SEEK_SET);
        fread(&curr, sizeof(Entry), 1, file);
        if (curr.tipo != 0){
            printEntry(curr, 0);
        }
        n++;
    }
}


void visit_entry(Entry entry, std :: vector <short> & visitado)
{
    if (entry.tipo != 0x10 && entry.tipo != 0x20)
        return;

    visitado.push_back(entry.primeiro_cluster);

    if (entry.tipo == 0x10)
    {
        Entry curr;
        curr.tipo = 0x10;
        int n = 2;
        int dirCluster = entry.primeiro_cluster;
        while (curr.tipo != 0 && n < bootRecord.entradas_root)
        {
            fseek(file, (dirCluster * BYTES_PER_CLUSTER) + (n * sizeof(Entry)), SEEK_SET);
            fread(&curr, sizeof(Entry), 1, file);
            visit_entry(curr, visitado);
            n++;
        }
    }
    
}

void checkCluster()
{
    // Passos para encontrar a cadeia de cluster
    //Achar se tem outro link para o próximo cluster no atual
    //Usar o valor lido da FAT para ler o próximo setor
    //Resumidademente
    //1-Extrair o valor do FAT para o cluster _current_. (Use a seção anterior na Tabela de Alocação de Arquivos para obter detalhes sobre como exatamente extrair o valor.) goto número 2
    //2-Este aglomerado está marcado como o último aglomerado da cadeia? (novamente, consulte a seção acima para obter mais detalhes) Sim, goto número 4. Não, goto número 3.
    //3-Leia o cluster representado pelo valor extraído e retorne para mais análises de diretórios. 
    //4-A extremidade da cadeia de aglomerados foi encontrada.

    Entry curr;
    curr.tipo = 0x10;
    int n = 0;

    std:: vector <short> inicios;

    while (n < bootRecord.entradas_root)
    {
        fseek(file, (START_ROOT_DIR * BYTES_PER_CLUSTER) + (n * sizeof(Entry)), SEEK_SET);
        fread(&curr, sizeof(Entry), 1, file);
        visit_entry(curr, inicios);
        //cout << " Primeira entrada do cluster: "<< curr.primeiro_cluster << endl;
        //inicios.push_back(curr.primeiro_cluster);
        n++;
    }

    std :: vector <unsigned short> visitados;

    for (auto &i : inicios) {

        unsigned short curr = i;
        visitados.push_back(curr);

        unsigned short next = searchInFAT(curr);
        while (next != 0xFFFE)
        {
            visitados.push_back(next);
            next = searchInFAT(next);
        }
    }

    // for(auto &cadeia : cadeias)
    // {
    //     short curr = cadeia[0];
    //     //short next;
    //     visitados.push_back(curr);
    //     unsigned short next = searchInFAT(curr);
    //     while(next != 0xFFFE)
    //     {
    //         //TODO Remover cadeia
    //         //cadeia.push_back(next);
    //         next = searchInFAT(next);
    //         visitados.push_back(next);
    //     //    cout<< next << endl;
    //     }
    //     // for(auto &i : cadeia) cout<< i << ' ';
    //     // cout << endl;
    // }

    //para cada item da fat
        //senao for vazio
            //senao estiver nos visitados
                //deu ruim

    std:: vector <unsigned short> abandonados;
    cout<< inicios.size() << " entradas registradas" << endl; 
    cout<< visitados.size() << " clusters visitados" << endl;
    //for ( auto &i : visitados) cout << i << endl;

    //unsigned short total_de_entradas = (bootRecord.bytes_por_setor/2) * bootRecord.setores_fat;//(bootRecord.setores_fat * bootRecord.bytes_por_setor)/2;
    //cout << total_de_entradas << endl;
    for(unsigned short i = 256 + 3; i < 0xFFFF; i++) //aqui
    {
        unsigned short curr = searchInFAT(i);

        if (curr == 0xFFFF) continue;
        if (curr == 0xFFFD) continue;
        //if (curr == 0) continue;
        //verifica se curr está dentro de visitados
        //caso não encontrado adicionar nos abandonados
        // if (curr == 0xFFFE) cout << "eoc" << endl;
        auto ress = find(visitados.begin(), visitados.end(), i);
        if(ress == visitados.end())//se nao encontrou e nao reservado
        {
            //cout << "[" << i << "] -> " << curr << endl;
            // cout << curr << endl;
            abandonados.push_back(i);
            //cout << "não encontrou nada" << endl;
            //não encontrou nada
        }
        

    }      

    cout<< " Foram encontrados " << abandonados.size() << " clusters abandonados" << endl;  
    
    

}


void menu()
{
    while (1)
    {
        unsigned int op;
        cout << "|--------------------------------------------------------|" << endl;
        cout << "| 1 - Formatar disco                                     |" << endl;
        cout << "| 2 - Copiar arquivo do disco para o sistema de arquivos |" << endl;
        cout << "| 3 - Copiar arquivo do sistema de arquivos para o disco |" << endl;
        cout << "| 4 - Listar arquivos e diretorios                       |" << endl;
        cout << "| 5 - Criar diretorio                                    |" << endl;
        cout << "| 6 - Verificar cadeias de cluster sem entradas          |" << endl;
        cout << "| 0 - Sair                                               |" << endl;
        cout << "|--------------------------------------------------------|" << endl;
        cout << "| Opcao desejada: ";

        cin >> op;

        switch (op)
        {
        case 1:
            openImage();
            format();
            fclose(file);
            break;
        case 2:
            openImage();
            selectFileToFSDestination();
            fclose(file);
            break;
        case 3:
            openImage();
            selectFileToDiskDestination();
            fclose(file);
            break;
        case 4:
            openImage();
            listAllFiles();
            fclose(file);
            break;
        case 5:
            openImage();
            selectMkDirDestination();
            fclose(file);
            break;
        case 6:
            openImage();
            checkCluster();
            fclose(file);
            break;
        case 0:
            cout << "| Saindo..." << endl;
            exit(0);
            break;
        default:
            cout << "| Comando invalido" << endl;
            menu();
            break;
        }
    }
}

int main()
{
    cout << "|--------------------------------------------------------|" << endl;
    cout << "|-------------        CAT FILE SYSTEM       -------------|" << endl;
    cout << "|--------------------------------------------------------|" << endl;
    cout << "|- Desenvolvido por:                                     |" << endl;
    cout << "|- Rodrigo Campos                                        |" << endl;
    cout << "|- Valquíria Belusso                                     |" << endl;
    cout << "|- Vitor Gilnek                                          |" << endl;
    menu();
    return 0;
}