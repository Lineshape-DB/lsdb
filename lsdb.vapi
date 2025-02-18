[CCode (cheader_filename = "lsdb.h", lower_case_cprefix = "lsdb_")]

namespace LSDB {
    public const int SUCCESS;
    public const int FAILURE;

    public const int OPEN_RO;
    public const int OPEN_RW;
    public const int OPEN_INIT;

    [Compact]
    [CCode (cname = "lsdb_model_t", destroy_function = "")]
    public struct Model {
        public ulong id;
        public string name;
        public string descr;
    }

    [CCode (cname = "lsdb_model_sink_t")]
    public delegate int ModelSink(Lsdb lsdb, Model m);

    [Compact]
    [CCode (cname = "lsdb_environment_t", destroy_function = "")]
    public struct Environment {
        public ulong id;
        public string name;
        public string descr;
    }

    [CCode (cname = "lsdb_environment_sink_t")]
    public delegate int EnvironmentSink(Lsdb lsdb, Environment e);

    [Compact]
    [CCode (cname = "lsdb_radiator_t", destroy_function = "")]
    public struct Radiator {
        public ulong id;
        public string sym;
        public uint anum;
        public double mass;
        public uint zsp;
    }

    [CCode (cname = "lsdb_radiator_sink_t")]
    public delegate int RadiatorSink(Lsdb lsdb, Radiator r);

    [Compact]
    [CCode (cname = "lsdb_line_t", destroy_function = "")]
    public struct Line {
        public ulong id;
        public string name;
        public double energy;
    }

    [CCode (cname = "lsdb_line_sink_t")]
    public delegate int LineSink(Lsdb lsdb, Line l);

    [Compact]
    [CCode (cname = "lsdb_dataset_t", destroy_function = "")]
    public struct Dataset {
        public ulong id;
        public ulong mid;
        public ulong eid;
        public double n;
        public double T;
    }

    [CCode (cname = "lsdb_dataset_sink_t")]
    public delegate int DatasetSink(Lsdb lsdb, Dataset ds);

    [Compact]
    [CCode (cname = "lsdb_dataset_data_t", cprefix = "lsdb_dataset_data_")]
    public class DatasetData {
        public double n;
        public double T;
        [CCode (array_length_cname = "len", array_length_type = "size_t")]
        public double[] x;
        [CCode (array_length_cname = "len", array_length_type = "size_t")]
        public double[] y;
    }

    [Compact]
    [CCode (cname = "lsdb_t", cprefix = "lsdb_", free_function = "lsdb_close")]
    public class Lsdb {
        [CCode (cname = "lsdb_open")]
        public Lsdb(string fname, int access);

        [CCode (cname = "lsdb_get_models")]
        public int get_models(ModelSink sink);

        [CCode (cname = "lsdb_get_environments")]
        public int get_environments(EnvironmentSink sink);

        [CCode (cname = "lsdb_get_radiators")]
        public int get_radiators(RadiatorSink sink);

        [CCode (cname = "lsdb_get_lines")]
        public int get_lines(ulong rid, LineSink sink);

        [CCode (cname = "lsdb_get_datasets")]
        public int get_datasets(ulong lid, DatasetSink sink);

        [CCode (cname = "lsdb_get_dataset_data")]
        public DatasetData? get_dataset_data(int did);

        [CCode (cname = "lsdb_get_interpolation")]
        public DatasetData? get_interpolation(ulong mid, ulong eid, ulong lid,
            double n, double T, ulong len);
    }
}
