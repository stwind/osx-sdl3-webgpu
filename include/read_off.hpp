#include <cstdio>
#include <string>
#include <iostream>

template <typename Scalar, typename Index>
inline bool readOFF(
  const std::string file_name,
  std::vector<Scalar>& V,
  std::vector<Index>& F)
{
  V.clear();
  F.clear();

  FILE* file = fopen(file_name.c_str(), "r");

  char header[1000];
  const std::string OFF("OFF");
  const std::string NOFF("NOFF");
  const std::string COFF("COFF");

  if (fscanf(file, "%s\n", header) != 1) {
    printf("readOFF() failed, invalid header: %s\n", header);
    fclose(file);
    return false;
  }
  if (!(std::string(header).compare(0, OFF.length(), OFF) == 0 ||
    std::string(header).compare(0, COFF.length(), COFF) == 0 ||
    std::string(header).compare(0, NOFF.length(), NOFF) == 0))
  {
    printf("Error: readOFF() first line should be OFF or NOFF or COFF, not %s...", header);
    fclose(file);
    return false;
  }

  int num_vertices, num_faces, num_edges;
  char tic_tac_toe;
  char line[1000];
  bool has_comment = true;
  while (has_comment) {
    fgets(line, 1000, file);
    has_comment = (line[0] == '#' || line[0] == '\n');
  }
  sscanf(line, "%d %d %d", &num_vertices, &num_faces, &num_edges);

  for (int i = 0;i < num_vertices;) {
    fgets(line, 1000, file);
    double x, y, z, nx, ny, nz;
    if (sscanf(line, "%lg %lg %lg %lg %lg %lg", &x, &y, &z, &nx, &ny, &nz) >= 3) {
      V.push_back(x);
      V.push_back(y);
      V.push_back(z);

      i++;
    }
    else if (fscanf(file, "%[#]", &tic_tac_toe) == 1) {
      char comment[1000];
      fscanf(file, "%[^\n]", comment);
    }
    else {
      printf("Error: bad line (%d)\n", i);
      if (feof(file)) {
        fclose(file);
        return false;
      }
    }
  }

  for (int i = 0;i < num_faces;)
  {
    int valence;
    if (fscanf(file, "%d", &valence) == 1) {
      for (int j = 0;j < valence;j++) {
        int index;
        if (j < valence - 1)
          fscanf(file, "%d", &index);
        else
          fscanf(file, "%d%*[^\n]", &index);

        F.push_back(index);
      }
      i++;
    }
    else if (fscanf(file, "%[#]", &tic_tac_toe) == 1) {
      char comment[1000];
      fscanf(file, "%[^\n]", comment);
    }
    else {
      printf("Error: bad line\n");
      fclose(file);
      return false;
    }
  }

  fclose(file);
  return true;
}