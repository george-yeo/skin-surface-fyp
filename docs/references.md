# References

Works the code, `CLAUDE.md` and the logs refer to. Keys are what the code
comments and logs use. **Verified** means the bibliographic details were
checked against the source named; anything else is marked and must be
checked before it goes into the report.

## Skin surfaces and meshing

**[KV07]** N. G. H. Kruithof, G. Vegter. *Meshing skin surfaces with
certified topology.* Computational Geometry: Theory and Applications 36(3),
166–182, 2007. doi:10.1016/j.comgeo.2006.01.003.
- Verified: Crossref record for the DOI (2026-10-17); full text read
  (author PDF, University of Groningen repository).
- Used by: `KruithofVegter.{hpp,cpp}` (anchors, tet complex, Condition 17,
  Thm 21); every "Lemma n / Condition 17 / Thm 21 / Observation 2 / Def. n"
  in the code and logs refers to this paper. Summary: log §3.2.

**[KV05]** N. G. H. Kruithof, G. Vegter. *Meshing skin surfaces with
certified topology.* Proc. 9th International CAD/Graphics Conference, 2005.
- Verified: as cited in CGAL's Skin_surface_3 documentation. Conference
  version of [KV07]; cite [KV07].

**[Ede99]** H. Edelsbrunner. *Deformable smooth surface design.* Discrete &
Computational Geometry 21, 87–115, 1999.
- Verified: [KV07] reference list (entry [15]). Not yet checked against the
  publisher.
- Used by: the skin surface definition, mixed complex, per-cell quadric
  (`SkinQuadric`).

**[CS04]** H.-L. Cheng, X. Shi. *Guaranteed quality triangulation of
molecular skin surfaces.* Proc. IEEE Visualization (VIS '04), 481–488, 2004.
- Verified: [KV07] reference list (entry [9]).
- Context: Prof. Cheng's work; [KV07] states it could not verify this
  method's homeomorphism claim (log §3.2). Relevant to next step 3
  (restricted Delaunay meshing).

**[CDES01]** H.-L. Cheng, T. K. Dey, H. Edelsbrunner, J. Sullivan.
*Dynamic skin triangulation.* Discrete & Computational Geometry 25,
525–568, 2001.
- **Unverified title:** the [KV07] text supplied (entry [8]) prints the
  title of [7] by mistake; authors, journal, volume and pages are from that
  entry. Check against the publisher.
- Relevance: dynamic skin meshing, i.e. the deformation extension.

## Mesh refinement (from [KV07] §3.4)

**[Che97]** L. Chew. *Guaranteed quality Delaunay meshing in 3D.* Proc.
13th Annual ACM Symposium on Computational Geometry, 391–393, 1997.
- Verified: [KV07] reference list (entry [10]).

**[Kob00]** L. Kobbelt. *√3-subdivision.* Proc. SIGGRAPH 2000, 103–112,
2000.
- Verified: [KV07] reference list (entry [19]).
- Used: `KvRefinement` (1→9 = two √3 steps, per [KV07] §3.4 Fig. 7). The
  paper itself not read; the "two steps = 1→9" structure is from [KV07].

## Algorithms used in the code

**[Eri05]** C. Ericson. *Real-Time Collision Detection.* Morgan Kaufmann,
2005. §5.1.5 (closest point on a triangle to a point).
- **Unverified:** publisher and year from memory.
- Used by: `closestOnTriangle` in `KruithofVegter.cpp`.

## Software

**[CGAL]** The CGAL Project. *CGAL, Computational Geometry Algorithms
Library*, https://www.cgal.org (version 6.2 via vcpkg).
- Used by: `PowerComplex` (`Regular_triangulation_3`, EPECK) and
  `CgalSkin` (`Skin_surface_3`, reference mesher). Cite the specific
  package manuals (3D Triangulations, 3D Skin Surface Meshing) in the
  report; their recommended citations are on cgal.org.
