import argparse
import falconn
import numpy as np
import sys
import time


def vector_from_hex_line(hex_line):
    b = bytes.fromhex(hex_line)
    return [val for val in b]


def read_database(filename, limit):
    db = []
    with open(filename) as f:
        for hex_line in f:
            vector = vector_from_hex_line(hex_line)
            if db and len(vector) != len(db[0]):
                raise ValueError(
                    f"All points should be of same length, first is {len(db[0])}, but {hex_line} is {len(vector)}"
                )
            db.append(vector)
            if len(db) == limit:
                break
    return np.array(db)


def save_numpy_database(db, mean, filename):
    np.savez(filename, db=db, mean=mean)


def read_numpy_database(filename):
    with np.load(filename, mmap_mode="r", allow_pickle=False) as data:
        return data["db"], data["mean"]


def test_query(db, query_object, query, mean):
    query_array = np.array(query, dtype=np.float32)
    query_array -= mean
    res = query_object.find_nearest_neighbor(query_array)
    dist = np.linalg.norm(db[res] - query_array)
    print(f"sq_dist={round(dist*dist)} idx={res}")


def test_queries(db, probes, mean, index_params, filename):
    start_build_index = time.monotonic_ns()
    index = falconn.LSHIndex(index_params)
    index.setup(dataset=db)
    end_build_index = time.monotonic_ns()

    print(
        f"Building index {(end_build_index-start_build_index) / 1000000.0:.3f}ms",
        file=sys.stderr,
    )

    query_object = index.construct_query_object()
    query_object.set_num_probes(probes)

    total_query_time = 0
    query_count = 0
    with open(filename) as f:
        for hex_line in f:
            query_count += 1
            vector = vector_from_hex_line(hex_line.strip())
            start_query = time.monotonic_ns()
            test_query(db, query_object, vector, mean)
            end_query = time.monotonic_ns()
            total_query_time += end_query - start_query
    nsperquery = total_query_time / query_count
    print(
        f"Total: {total_query_time / 1000000.0:.3f}ms, {query_count} items, avg {nsperquery / 1000000.0:.3f}ms per query, {1000000000.0 / nsperquery:.3f} qps",
        file=sys.stderr,
    )


def hyperplane_hashing_params(dimensions):
    params_hp = falconn.LSHConstructionParameters()
    params_hp.dimension = dimensions
    params_hp.lsh_family = falconn.LSHFamily.Hyperplane
    params_hp.distance_function = falconn.DistanceFunction.EuclideanSquared
    params_hp.storage_hash_table = falconn.StorageHashTable.FlatHashTable
    params_hp.k = 19
    params_hp.l = 10
    params_hp.num_setup_threads = 0

    return params_hp


def cross_polytope_hashing_params(dimensions):
    params_cp = falconn.LSHConstructionParameters()
    params_cp.dimension = dimensions
    params_cp.lsh_family = falconn.LSHFamily.CrossPolytope
    params_cp.distance_function = falconn.DistanceFunction.EuclideanSquared
    params_cp.storage_hash_table = falconn.StorageHashTable.FlatHashTable
    params_cp.k = 3
    params_cp.l = 10
    params_cp.num_setup_threads = 0
    params_cp.last_cp_dimension = 16
    params_cp.num_rotations = 3

    return params_cp


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--database", metavar="FILENAME", default="database.txt")
    mode.add_argument("--numpy-database", metavar="FILENAME")

    parser.add_argument("--test-vector", metavar="FILENAME", default="test-vector.txt")
    parser.add_argument("--limit", metavar="MAX", type=int, default=-1)
    parser.add_argument(
        "--params",
        choices=("hyperplane", "crosspolytope", "default"),
        default="default",
    )

    parser.add_argument("--probes", type=int, default=2464)

    args = parser.parse_args()

    start_read_db = time.monotonic_ns()
    if args.numpy_database:
        db, mean = read_numpy_database(args.numpy_database)
    else:
        db = read_database(args.database, args.limit)
        db = db.astype(np.float32)
        mean = np.mean(db, axis=0)
        db -= mean
        save_numpy_database(db, mean, args.database)
    end_read_db = time.monotonic_ns()

    print(
        f"Reading database {(end_read_db-start_read_db) / 1000000.0:.3f}ms",
        file=sys.stderr,
    )

    num_points = len(db)
    dimensions = len(db[0])

    if args.params == "default":
        index_params = falconn.get_default_parameters(
            num_points=num_points, dimension=dimensions
        )
    elif args.params == "hyperplane":
        index_params = hyperplane_hashing_params(dimensions=dimensions)
    elif args.params == "crosspolytope":
        index_params = cross_polytope_hashing_params(dimensions=dimensions)
    else:
        raise ValueError(f"Unknown params: {args.params}")

    test_queries(db, args.probes, mean, index_params, args.test_vector)


if __name__ == "__main__":
    sys.exit(main())
