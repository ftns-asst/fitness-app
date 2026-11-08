package model

import (
	"fmt"
	"ftns-asst/internal/util"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/google/uuid"
)

type (
	MuscleGroupKey string
	MuscleGroup    struct {
		Key  MuscleGroupKey // unique key
		Name string         // readable name
	}

	EquipmentKey string
	Equipment    struct {
		Key  EquipmentKey // unique key
		Name string       // readable name
	}

	Exercise struct {
		ID           uuid.UUID        `db:"id"`
		OwnerID      *uuid.UUID       `db:"owner_id"`
		MediaID      *uuid.UUID       `db:"media_id"`
		Media        *Media           `db:"-"`
		IsPublic     bool             `db:"is_public"`
		Name         string           `db:"name"`
		Description  *string          `db:"description"`
		MuscleGroups []MuscleGroupKey `db:"muscle_group"`
		Equipments   []EquipmentKey   `db:"equipment"`
		CreatedAt    time.Time        `db:"created_at"`
		DeletedAt    *time.Time       `db:"deleted_at"`
	}
	ExerciseListType string
	ExerciseFilters  struct {
		ListType     ExerciseListType
		UserID       *uuid.UUID       // for own and user list types
		MuscleGroups []MuscleGroupKey // filter by muscle groups
		Equipments   []EquipmentKey   // filter by available equipments
		Search       *string
	}

	ExerciseInput struct {
		IsPublic     *bool
		Name         *string
		Description  *string
		MuscleGroups []MuscleGroupKey
		Equipments   []EquipmentKey
	}

	UpdateExerciseInput struct {
		ID uuid.UUID
		ExerciseInput
	}

	// readonly reader for muscle groups
	MuscleGroupProvider util.DataReader[MuscleGroup]
	// readonly reader for equipment
	EquipmentProvider util.DataReader[Equipment]

	ExerciseMedia struct {
		ExerciseID uuid.UUID `db:"exercise_id"`
		Type       MediaType `db:"type"`
		ObjectKey  string    `db:"object_key"`
	}
)

const (
	ExerciseListTypeAll        ExerciseListType = "all"         // basic + own + public
	ExerciseListTypeOwn        ExerciseListType = "own"         // only own
	ExerciseListTypePublic     ExerciseListType = "public"      // all public
	ExerciseListTypePublicUser ExerciseListType = "public_user" // only public exercises of user
	ExerciseListTypeBasic      ExerciseListType = "basic"       // only basic
)

var (
	ErrUserIDRequired = fmt.Errorf("user id is required for this list type: %w", ErrBadRequest)
)

// max length of exercises.name column
const ExerciseNameMaxLength = 255

func validateExerciseName(name string) error {
	if strings.TrimSpace(name) == "" {
		return fmt.Errorf("exercise name must be not blank: %w", ErrBadRequest)
	}
	if utf8.RuneCountInString(name) > ExerciseNameMaxLength {
		return fmt.Errorf("exercise name must be at most %d characters long: %w", ExerciseNameMaxLength, ErrBadRequest)
	}
	return nil
}

func validateMuscleGroups(groups []MuscleGroupKey, allowed MuscleGroupProvider) error {
	for _, group := range groups {
		if _, ok := allowed.Get(string(group)); !ok {
			return fmt.Errorf("unknown muscle group %q: %w", group, ErrBadRequest)
		}
	}
	return nil
}

func validateEquipments(equipments []EquipmentKey, allowed EquipmentProvider) error {
	for _, equipment := range equipments {
		if _, ok := allowed.Get(string(equipment)); !ok {
			return fmt.Errorf("unknown equipment %q: %w", equipment, ErrBadRequest)
		}
	}
	return nil
}

func (e *Exercise) Validate(muscleGroupProvider MuscleGroupProvider, equipmentProvider EquipmentProvider) error {
	if err := validateExerciseName(e.Name); err != nil {
		return err
	}
	if err := validateMuscleGroups(e.MuscleGroups, muscleGroupProvider); err != nil {
		return err
	}
	return validateEquipments(e.Equipments, equipmentProvider)
}

// validate partial update, nil fields are not changed
func (i *ExerciseInput) Validate(muscleGroupProvider MuscleGroupProvider, equipmentProvider EquipmentProvider) error {
	if i.Name != nil {
		if err := validateExerciseName(*i.Name); err != nil {
			return err
		}
	}
	if err := validateMuscleGroups(i.MuscleGroups, muscleGroupProvider); err != nil {
		return err
	}
	return validateEquipments(i.Equipments, equipmentProvider)
}

func (e *ExerciseFilters) ValidateAndDedup() error {
	if e.UserID == nil && (e.ListType == ExerciseListTypeOwn || e.ListType == ExerciseListTypePublicUser) {
		return ErrUserIDRequired
	}

	e.MuscleGroups = util.Dedup(e.MuscleGroups, func(m MuscleGroupKey) MuscleGroupKey { return m })
	e.Equipments = util.Dedup(e.Equipments, func(e EquipmentKey) EquipmentKey { return e })
	return nil
}

const (
	MuscleGroupChest      MuscleGroupKey = "chest"       // грудные
	MuscleGroupUpperBack  MuscleGroupKey = "upper_back"  // верх спины
	MuscleGroupLowerBack  MuscleGroupKey = "lower_back"  // широчайшие
	MuscleGroupLats       MuscleGroupKey = "lats"        // поясница
	MuscleGroupFrontDelts MuscleGroupKey = "front_delts" // передние дельты
	MuscleGroupSideDelts  MuscleGroupKey = "side_delts"  // средние дельты
	MuscleGroupRearDelts  MuscleGroupKey = "rear_delts"  // задние дельты
	MuscleGroupBiceps     MuscleGroupKey = "biceps"      // бицепс
	MuscleGroupTriceps    MuscleGroupKey = "triceps"     // трицепс
	MuscleGroupTraps      MuscleGroupKey = "traps"       // трапеции
	MuscleGroupForearms   MuscleGroupKey = "forearms"    // предплечья
	MuscleGroupAbs        MuscleGroupKey = "abs"         // пресс
	MuscleGroupOblique    MuscleGroupKey = "obliques"    // косые мышцы живота
	MuscleGroupHamstrings MuscleGroupKey = "hamstrings"  // бицепс бедра
	MuscleGroupGlutes     MuscleGroupKey = "glutes"      // ягодицы
	MuscleGroupQuads      MuscleGroupKey = "quads"       // квадрицепсы
	MuscleGroupAdductors  MuscleGroupKey = "adductors"   // приводящие мышцы бедра
	MuscleGroupAbductors  MuscleGroupKey = "abductors"   // отводящие мышцы бедра
	MuscleGroupCalves     MuscleGroupKey = "calves"      // икры
	MuscleGroupCardio     MuscleGroupKey = "cardio"      // кардио (сердечно-сосудистая система)
)

var TestMuscleGroupList = NewSet(
	MuscleGroupChest,
	MuscleGroupUpperBack,
	MuscleGroupLowerBack,
	MuscleGroupLats,
	MuscleGroupFrontDelts,
	MuscleGroupSideDelts,
	MuscleGroupRearDelts,
	MuscleGroupBiceps,
	MuscleGroupTriceps,
	MuscleGroupTraps,
	MuscleGroupForearms,
	MuscleGroupAbs,
	MuscleGroupOblique,
	MuscleGroupHamstrings,
	MuscleGroupGlutes,
	MuscleGroupQuads,
	MuscleGroupAdductors,
	MuscleGroupAbductors,
	MuscleGroupCalves,
	MuscleGroupCardio,
)

const (
	EquipmentBodyweight          EquipmentKey = "bodyweight"            // Собственный вес
	EquipmentBarbell             EquipmentKey = "barbell"               // Штанга
	EquipmentEZBar               EquipmentKey = "ez_bar"                // EZ-гриф
	EquipmentDumbbell            EquipmentKey = "dumbbell"              // Гантели
	EquipmentKettlebell          EquipmentKey = "kettlebell"            // Гиря
	EquipmentBench               EquipmentKey = "bench"                 // Скамья
	EquipmentInclineBench        EquipmentKey = "incline_bench"         // Наклонная скамья
	EquipmentSquatRack           EquipmentKey = "squat_rack"            // Силовая рама / стойки
	EquipmentSmithMachine        EquipmentKey = "smith_machine"         // Машина Смита
	EquipmentCableMachine        EquipmentKey = "cable_machine"         // Блочный тренажёр (кроссовер)
	EquipmentLatPulldownMachine  EquipmentKey = "lat_pulldown_machine"  // Тренажёр верхней тяги
	EquipmentSeatedRowMachine    EquipmentKey = "seated_row_machine"    // Тренажёр горизонтальной тяги
	EquipmentLegPressMachine     EquipmentKey = "leg_press_machine"     // Тренажёр для жима ногами
	EquipmentLegExtensionMachine EquipmentKey = "leg_extension_machine" // Тренажёр для разгибания ног
	EquipmentLegCurlMachine      EquipmentKey = "leg_curl_machine"      // Тренажёр для сгибания ног
	EquipmentChestPressMachine   EquipmentKey = "chest_press_machine"   // Тренажёр для жима от груди
	EquipmentPecDeckMachine      EquipmentKey = "pec_deck_machine"      // Тренажёр «Бабочка»
	EquipmentHackSquatMachine    EquipmentKey = "hack_squat_machine"    // Гакк-тренажёр
	EquipmentCalfRaiseMachine    EquipmentKey = "calf_raise_machine"    // Тренажёр для икр
	EquipmentPullUpBar           EquipmentKey = "pull_up_bar"           // Турник
	EquipmentDipBars             EquipmentKey = "dip_bars"              // Брусья
	EquipmentRomanChair          EquipmentKey = "roman_chair"           // Римский стул / гиперэкстензия
	EquipmentResistanceBand      EquipmentKey = "resistance_band"       // Резиновая лента
	EquipmentMedicineBall        EquipmentKey = "medicine_ball"         // Медбол
	EquipmentAbWheel             EquipmentKey = "ab_wheel"              // Ролик для пресса
	EquipmentJumpRope            EquipmentKey = "jump_rope"             // Скакалка
	EquipmentTreadmill           EquipmentKey = "treadmill"             // Беговая дорожка
	EquipmentExerciseBike        EquipmentKey = "exercise_bike"         // Велотренажёр
	EquipmentRowingMachine       EquipmentKey = "rowing_machine"        // Гребной тренажёр
	EquipmentElliptical          EquipmentKey = "elliptical"            // Эллипсоид
	EquipmentMat                 EquipmentKey = "mat"                   // Коврик
)

var TestEquipmentList = NewSet(
	EquipmentBodyweight,
	EquipmentBarbell,
	EquipmentEZBar,
	EquipmentDumbbell,
	EquipmentKettlebell,
	EquipmentBench,
	EquipmentInclineBench,
	EquipmentSquatRack,
	EquipmentSmithMachine,
	EquipmentCableMachine,
	EquipmentLatPulldownMachine,
	EquipmentSeatedRowMachine,
	EquipmentLegPressMachine,
	EquipmentLegExtensionMachine,
	EquipmentLegCurlMachine,
	EquipmentChestPressMachine,
	EquipmentPecDeckMachine,
	EquipmentHackSquatMachine,
	EquipmentCalfRaiseMachine,
	EquipmentPullUpBar,
	EquipmentDipBars,
	EquipmentRomanChair,
	EquipmentResistanceBand,
	EquipmentMedicineBall,
	EquipmentAbWheel,
	EquipmentJumpRope,
	EquipmentTreadmill,
	EquipmentExerciseBike,
	EquipmentRowingMachine,
	EquipmentElliptical,
	EquipmentMat,
)

// muscle groups provider with all keys from TestMuscleGroupList
func NewTestMuscleGroupProvider() MuscleGroupProvider {
	items := make([]MuscleGroup, 0, len(TestMuscleGroupList))
	for key := range TestMuscleGroupList {
		items = append(items, MuscleGroup{Key: key, Name: string(key)})
	}
	return util.NewDataReader(items, func(m MuscleGroup) string { return string(m.Key) })
}

// equipment provider with all keys from TestEquipmentList
func NewTestEquipmentProvider() EquipmentProvider {
	items := make([]Equipment, 0, len(TestEquipmentList))
	for key := range TestEquipmentList {
		items = append(items, Equipment{Key: key, Name: string(key)})
	}
	return util.NewDataReader(items, func(e Equipment) string { return string(e.Key) })
}

var createdAt = time.Date(2026, time.January, 1, 0, 0, 0, 0, time.UTC)

var TestExercises = []*Exercise{
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e01"),
		IsPublic:     true,
		Name:         "Жим штанги лёжа",
		MuscleGroups: []MuscleGroupKey{MuscleGroupChest, MuscleGroupTriceps, MuscleGroupFrontDelts},
		Equipments:   []EquipmentKey{EquipmentBarbell, EquipmentBench},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e02"),
		IsPublic:     true,
		Name:         "Приседания со штангой",
		MuscleGroups: []MuscleGroupKey{MuscleGroupQuads, MuscleGroupGlutes, MuscleGroupHamstrings},
		Equipments:   []EquipmentKey{EquipmentBarbell, EquipmentSquatRack},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e03"),
		IsPublic:     true,
		Name:         "Становая тяга",
		MuscleGroups: []MuscleGroupKey{MuscleGroupLowerBack, MuscleGroupHamstrings, MuscleGroupGlutes, MuscleGroupTraps},
		Equipments:   []EquipmentKey{EquipmentBarbell},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e04"),
		IsPublic:     true,
		Name:         "Подтягивания",
		MuscleGroups: []MuscleGroupKey{MuscleGroupLats, MuscleGroupBiceps, MuscleGroupUpperBack},
		Equipments:   []EquipmentKey{EquipmentPullUpBar},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e05"),
		IsPublic:     true,
		Name:         "Жим гантелей сидя",
		MuscleGroups: []MuscleGroupKey{MuscleGroupFrontDelts, MuscleGroupSideDelts, MuscleGroupTriceps},
		Equipments:   []EquipmentKey{EquipmentDumbbell, EquipmentBench},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e06"),
		IsPublic:     true,
		Name:         "Отжимания",
		MuscleGroups: []MuscleGroupKey{MuscleGroupChest, MuscleGroupTriceps, MuscleGroupFrontDelts},
		Equipments:   []EquipmentKey{EquipmentBodyweight},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e07"),
		IsPublic:     true,
		Name:         "Сгибание рук с гантелями",
		MuscleGroups: []MuscleGroupKey{MuscleGroupBiceps, MuscleGroupForearms},
		Equipments:   []EquipmentKey{EquipmentDumbbell},
		CreatedAt:    createdAt,
	},
	{
		ID:           uuid.MustParse("6f1c2d3e-4a5b-4c6d-8e7f-9a0b1c2d3e08"),
		IsPublic:     true,
		Name:         "Скручивания на ролике",
		MuscleGroups: []MuscleGroupKey{MuscleGroupAbs, MuscleGroupOblique},
		Equipments:   []EquipmentKey{EquipmentAbWheel, EquipmentMat},
		CreatedAt:    createdAt,
	},
}
