# Solver experiment guides

## Goal

Rewrite `agent.md` and `plan.md` from the audited CASHWMaxSAT and SPBMAXSAT2 sources, then deliver matching copies to the server project.

### Phase 1: Audit source and existing guides

**Status:** complete

- Read the existing documents and both solver interfaces.
- Verified CASH output behavior and SPB build path.
- Found and reproduced the SPB standard-WCNF parsing defect.

### Phase 2: Rewrite guidance

**Status:** complete

- Rewrote local `agent.md` and `plan.md` around verified interfaces and the Parser Gate.

### Phase 3: Server delivery and verification

**Status:** complete

- Uploaded both files to `/home/fwkzj/HybridAlgorithm` and compared SHA-256 hashes.

## Next Step

The rewritten guides are delivered and their server hashes match the local copies.

## Errors Encountered

| Error | Resolution |
|---|---|
| SPB reported zero for a CASH sample whose exact optimum is 1611 | Traced to WCNF parser allocation and hard-clause handling; documented as Gate 0 |
| Initial patch attempted delete/add of the same file in one patch | Replaced files in separate patch operations |
